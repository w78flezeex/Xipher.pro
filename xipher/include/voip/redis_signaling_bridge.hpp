#ifndef REDIS_SIGNALING_BRIDGE_HPP
#define REDIS_SIGNALING_BRIDGE_HPP

#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <map>

namespace xipher {

/**
 * Redis Signaling Bridge for Scalable VoIP Signaling
 * 
 * Enables cross-server signaling using Redis Pub/Sub:
 * - Each server instance subscribes to Redis channels
 * - Messages are published to Redis when target user is on different server
 * - Subscribed servers forward messages to their local WebSocket connections
 * 
 * Architecture:
 *   Server A (User 1) -> Redis Channel -> Server B (User 2)
 */
class RedisSignalingBridge {
public:
    using MessageHandler = std::function<void(const std::string& user_id, const std::string& message)>;
    
    /**
     * Initialize Redis connection
     * @param redis_host Redis server hostname
     * @param redis_port Redis server port
     * @param server_id Unique identifier for this server instance
     */
    RedisSignalingBridge(const std::string& redis_host = "localhost",
                        int redis_port = 6379,
                        const std::string& server_id = "");
    
    ~RedisSignalingBridge();
    
    /**
     * Start the Redis subscriber thread
     */
    bool start();
    
    /**
     * Stop the Redis subscriber
     */
    void stop();
    
    /**
     * Publish a signaling message to Redis
     * @param target_user_id User to receive the message
     * @param message JSON signaling message
     * @return true if published successfully
     */
    bool publishMessage(const std::string& target_user_id, const std::string& message);
    
    /**
     * Register a local user connection
     * When a message arrives for this user, the handler will be called
     */
    void registerLocalUser(const std::string& user_id, MessageHandler handler);
    
    /**
     * Unregister a local user connection
     */
    void unregisterLocalUser(const std::string& user_id);
    
    /**
     * Check if a user is connected to this server instance
     */
    bool isUserLocal(const std::string& user_id) const;
    
    /**
     * Get server instance ID
     */
    std::string getServerId() const { return server_id_; }

private:
    std::string redis_host_;
    int redis_port_;
    std::string server_id_;
    
    std::atomic<bool> running_;
    std::thread subscriber_thread_;
    
    // Local user connections (user_id -> handler)
    std::map<std::string, MessageHandler> local_users_;
    mutable std::mutex local_users_mutex_;
    
    // Redis connection handles (simplified - in production use hiredis)
    void* redis_context_; // Will be redisContext* from hiredis
    
    void subscriberLoop();
    void handleRedisMessage(const std::string& channel, const std::string& message);
    
    // Generate Redis channel name for a user
    std::string getUserChannel(const std::string& user_id) const;
};

} // namespace xipher

#endif // REDIS_SIGNALING_BRIDGE_HPP
