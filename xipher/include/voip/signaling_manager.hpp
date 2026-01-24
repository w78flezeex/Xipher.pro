#ifndef SIGNALING_MANAGER_HPP
#define SIGNALING_MANAGER_HPP

#include <string>
#include <map>
#include <functional>
#include <memory>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/beast/websocket.hpp>
#include "../voip/redis_signaling_bridge.hpp"

namespace xipher {
namespace voip {

/**
 * Scalable Signaling Manager for VoIP
 * 
 * Handles WebRTC signaling (Offer/Answer/ICE) for 10,000+ concurrent connections
 * Uses Redis Pub/Sub for cross-server communication
 */
class SignalingManager {
public:
    using WebSocketPtr = std::shared_ptr<boost::beast::websocket::stream<boost::asio::ip::tcp::socket>>;
    using MessageHandler = std::function<void(const std::string& user_id, const std::string& message)>;
    
    struct Config {
        std::string redis_host = "localhost";
        int redis_port = 6379;
        std::string server_id = ""; // Unique ID for this server instance
        int max_connections = 10000;
    };
    
    SignalingManager();
    explicit SignalingManager(const Config& config);
    ~SignalingManager();
    
    /**
     * Initialize the signaling manager
     */
    bool initialize();
    
    /**
     * Register a WebSocket connection for a user
     */
    void registerConnection(const std::string& user_id, WebSocketPtr ws);
    
    /**
     * Unregister a WebSocket connection
     */
    void unregisterConnection(const std::string& user_id);
    
    /**
     * Handle incoming signaling message
     */
    void handleMessage(const std::string& user_id, const std::string& message);
    
    /**
     * Send signaling message to a user
     */
    bool sendMessage(const std::string& user_id, const std::string& message);
    
    /**
     * Check if a user is connected to this server
     */
    bool isUserConnected(const std::string& user_id) const;
    
    /**
     * Get number of active connections
     */
    size_t getConnectionCount() const;

private:
    Config config_;
    
    // Local WebSocket connections (user_id -> websocket)
    std::map<std::string, WebSocketPtr> local_connections_;
    mutable std::mutex connections_mutex_;
    
    // Redis bridge for cross-server signaling
    std::unique_ptr<RedisSignalingBridge> redis_bridge_;
    
    // Message parsing and routing
    void routeMessage(const std::string& from_user_id, 
                     const std::map<std::string, std::string>& message);
    
    // Extract target user from message
    std::string extractTargetUser(const std::map<std::string, std::string>& message) const;
};

} // namespace voip
} // namespace xipher

#endif // SIGNALING_MANAGER_HPP
