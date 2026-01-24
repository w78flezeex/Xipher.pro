#include "../../include/voip/signaling_manager.hpp"
#include "../../include/voip/signaling_protocol.hpp"
#include "../../include/utils/json_parser.hpp"
#include "../../include/utils/logger.hpp"
#include <boost/beast/core.hpp>
#include <boost/asio.hpp>

namespace xipher {
namespace voip {

SignalingManager::SignalingManager()
    : config_() {}

SignalingManager::SignalingManager(const Config& config)
    : config_(config) {}

SignalingManager::~SignalingManager() {
    if (redis_bridge_) {
        redis_bridge_->stop();
    }
}

bool SignalingManager::initialize() {
    // Initialize Redis bridge
    redis_bridge_ = std::make_unique<RedisSignalingBridge>(
        config_.redis_host,
        config_.redis_port,
        config_.server_id
    );
    
    // Register handler for incoming Redis messages
    redis_bridge_->registerLocalUser("", [this](const std::string& user_id, const std::string& message) {
        // Forward message to local WebSocket connection
        sendMessage(user_id, message);
    });
    
    if (!redis_bridge_->start()) {
        Logger::getInstance().error("Failed to start Redis signaling bridge");
        return false;
    }
    
    Logger::getInstance().info("SignalingManager initialized: server_id=" + config_.server_id);
    return true;
}

void SignalingManager::registerConnection(const std::string& user_id, WebSocketPtr ws) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    local_connections_[user_id] = ws;
    
    // Register with Redis bridge
    if (redis_bridge_) {
        redis_bridge_->registerLocalUser(user_id, [this, user_id](const std::string&, const std::string& message) {
            sendMessage(user_id, message);
        });
    }
    
    Logger::getInstance().info("Registered signaling connection: " + user_id + 
                              " (total: " + std::to_string(local_connections_.size()) + ")");
}

void SignalingManager::unregisterConnection(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    local_connections_.erase(user_id);
    
    // Unregister from Redis bridge
    if (redis_bridge_) {
        redis_bridge_->unregisterLocalUser(user_id);
    }
    
    Logger::getInstance().info("Unregistered signaling connection: " + user_id);
}

void SignalingManager::handleMessage(const std::string& user_id, const std::string& message) {
    try {
        auto data = JsonParser::parse(message);
        std::string type = data["type"];
        
        // Route message to target user
        routeMessage(user_id, data);
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("Error handling signaling message: " + std::string(e.what()));
        
        // Send error response
        std::string error = SignalingProtocol::createError(
            SignalingProtocol::ErrorCode::INVALID_MESSAGE,
            "Failed to parse message: " + std::string(e.what())
        );
        sendMessage(user_id, error);
    }
}

bool SignalingManager::sendMessage(const std::string& user_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    
    auto it = local_connections_.find(user_id);
    if (it != local_connections_.end()) {
        auto ws = it->second;
        if (ws) {
            // Send via WebSocket
            boost::beast::error_code ec;
            ws->write(boost::asio::buffer(message), ec);
            
            if (ec) {
                Logger::getInstance().error("Failed to send WebSocket message to " + user_id + 
                                          ": " + ec.message());
                return false;
            }
            
            return true;
        }
    }
    
    // User not on this server - try Redis
    if (redis_bridge_) {
        return redis_bridge_->publishMessage(user_id, message);
    }
    
    Logger::getInstance().warning("User not found for signaling: " + user_id);
    return false;
}

bool SignalingManager::isUserConnected(const std::string& user_id) const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    return local_connections_.find(user_id) != local_connections_.end();
}

size_t SignalingManager::getConnectionCount() const {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    return local_connections_.size();
}

void SignalingManager::routeMessage(const std::string& from_user_id, 
                                    const std::map<std::string, std::string>& message) {
    std::string target_user_id = extractTargetUser(message);
    
    if (target_user_id.empty()) {
        Logger::getInstance().warning("No target user in signaling message from: " + from_user_id);
        return;
    }
    
    // Reconstruct message with from_user_id
    std::string message_json = JsonParser::stringify(message);
    
    // Add from_user_id if not present
    if (message.find("from_user_id") == message.end()) {
        // Simple JSON manipulation - in production use proper JSON library
        size_t pos = message_json.find_last_of('}');
        if (pos != std::string::npos) {
            message_json.insert(pos, ",\"from_user_id\":\"" + from_user_id + "\"");
        }
    }
    
    // Send to target user
    sendMessage(target_user_id, message_json);
}

std::string SignalingManager::extractTargetUser(const std::map<std::string, std::string>& message) const {
    // Try different field names
    if (message.find("callee_id") != message.end()) {
        return message.at("callee_id");
    }
    if (message.find("target_user_id") != message.end()) {
        return message.at("target_user_id");
    }
    if (message.find("receiver_id") != message.end()) {
        return message.at("receiver_id");
    }
    if (message.find("caller_id") != message.end()) {
        // For answer/ice, target is the caller
        return message.at("caller_id");
    }
    return "";
}

} // namespace voip
} // namespace xipher
