#ifndef IN_MEMORY_STORAGE_HPP
#define IN_MEMORY_STORAGE_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <chrono>
#include <algorithm>
#include "../database/db_manager.hpp"

namespace xipher {

/**
 * In-Memory Storage - полная замена базы данных
 * Thread-safe хранилище всех данных в памяти
 * Обеспечивает все функции DatabaseManager без БД
 */
class InMemoryStorage {
public:
    // Singleton pattern для глобального доступа
    static InMemoryStorage& getInstance();
    
    // Инициализация хранилища
    bool initialize();
    
    // ========== USER OPERATIONS ==========
    bool createUser(const std::string& username, const std::string& password_hash, std::string& user_id);
    User getUserByUsername(const std::string& username);
    User getUserById(const std::string& user_id);
    bool usernameExists(const std::string& username);
    bool updateLastLogin(const std::string& user_id);
    bool updateLastActivity(const std::string& user_id);
    std::string getUserLastActivity(const std::string& user_id);
    bool isUserOnline(const std::string& user_id, int threshold_seconds = 300);
    
    // ========== FRIEND OPERATIONS ==========
    bool createFriendRequest(const std::string& sender_id, const std::string& receiver_id, std::string& request_id);
    bool acceptFriendRequest(const std::string& request_id);
    bool rejectFriendRequest(const std::string& request_id);
    std::vector<FriendRequest> getFriendRequests(const std::string& user_id);
    bool areFriends(const std::string& user1_id, const std::string& user2_id);
    std::vector<Friend> getFriends(const std::string& user_id);
    
    // ========== MESSAGE OPERATIONS ==========
    bool sendMessage(const std::string& sender_id, const std::string& receiver_id, const std::string& content,
                     const std::string& message_type = "text", const std::string& file_path = "",
                     const std::string& file_name = "", long long file_size = 0, 
                     const std::string& reply_to_message_id = "",
                     const std::string& forwarded_from_user_id = "", 
                     const std::string& forwarded_from_username = "",
                     const std::string& forwarded_from_message_id = "",
                     std::string* message_id = nullptr);
    std::vector<Message> getMessages(const std::string& user1_id, const std::string& user2_id, int limit = 50);
    Message getLastMessage(const std::string& user1_id, const std::string& user2_id);
    Message getMessageById(const std::string& message_id);
    bool editMessage(const std::string& message_id, const std::string& new_content);
    bool deleteMessage(const std::string& message_id);
    int getUnreadCount(const std::string& user_id, const std::string& sender_id);
    bool markMessagesAsRead(const std::string& user_id, const std::string& sender_id);
    std::vector<Friend> getChatPartners(const std::string& user_id);
    
    // ========== GROUP OPERATIONS ==========
    bool createGroup(const std::string& name, const std::string& description, 
                     const std::string& creator_id, std::string& group_id);
    DatabaseManager::Group getGroupById(const std::string& group_id);
    std::vector<DatabaseManager::Group> getUserGroups(const std::string& user_id);
    bool addGroupMember(const std::string& group_id, const std::string& user_id, const std::string& role = "member");
    bool removeGroupMember(const std::string& group_id, const std::string& user_id);
    bool updateGroupMemberRole(const std::string& group_id, const std::string& user_id, const std::string& role);
    bool muteGroupMember(const std::string& group_id, const std::string& user_id, bool muted);
    bool banGroupMember(const std::string& group_id, const std::string& user_id, bool banned, const std::string& until = "");
    std::vector<DatabaseManager::GroupMember> getGroupMembers(const std::string& group_id);
    DatabaseManager::GroupMember getGroupMember(const std::string& group_id, const std::string& user_id);
    bool sendGroupMessage(const std::string& group_id, const std::string& sender_id, const std::string& content,
                         const std::string& message_type = "text", const std::string& file_path = "",
                         const std::string& file_name = "", long long file_size = 0,
                         const std::string& reply_to_message_id = "",
                         const std::string& forwarded_from_user_id = "", 
                         const std::string& forwarded_from_username = "",
                         const std::string& forwarded_from_message_id = "",
                         std::string* message_id = nullptr);
    std::vector<DatabaseManager::GroupMessage> getGroupMessages(const std::string& group_id, int limit = 50);
    bool pinGroupMessage(const std::string& group_id, const std::string& message_id, const std::string& user_id);
    bool unpinGroupMessage(const std::string& group_id, const std::string& message_id);
    std::string createGroupInviteLink(const std::string& group_id, const std::string& creator_id, 
                                      int expires_in_seconds = 0);
    // Returns group_id joined, or empty on failure
    std::string joinGroupByInviteLink(const std::string& invite_link, const std::string& user_id);
    bool updateGroupName(const std::string& group_id, const std::string& new_name);
    bool updateGroupDescription(const std::string& group_id, const std::string& new_description);
    
    // ========== CHANNEL OPERATIONS ==========
    bool createChannel(const std::string& name, const std::string& description, 
                      const std::string& creator_id, std::string& channel_id, 
                      const std::string& custom_link = "");
    bool checkChannelCustomLinkExists(const std::string& custom_link);
    DatabaseManager::Channel getChannelById(const std::string& channel_id);
    DatabaseManager::Channel getChannelByCustomLink(const std::string& custom_link);
    std::vector<DatabaseManager::Channel> getUserChannels(const std::string& user_id);
    bool addChannelMember(const std::string& channel_id, const std::string& user_id, 
                         const std::string& role = "subscriber");
    bool removeChannelMember(const std::string& channel_id, const std::string& user_id);
    bool updateChannelMemberRole(const std::string& channel_id, const std::string& user_id, const std::string& role);
    bool banChannelMember(const std::string& channel_id, const std::string& user_id, bool banned);
    std::vector<DatabaseManager::ChannelMember> getChannelMembers(const std::string& channel_id);
    DatabaseManager::ChannelMember getChannelMember(const std::string& channel_id, const std::string& user_id);
    int countChannelSubscribers(const std::string& channel_id);
    int countChannelMembers(const std::string& channel_id);
    bool sendChannelMessage(const std::string& channel_id, const std::string& sender_id, const std::string& content,
                           const std::string& message_type = "text", const std::string& file_path = "",
                           const std::string& file_name = "", long long file_size = 0,
                           std::string* message_id = nullptr);
    std::vector<DatabaseManager::ChannelMessage> getChannelMessages(const std::string& channel_id, int limit = 50);
    bool pinChannelMessage(const std::string& channel_id, const std::string& message_id, const std::string& user_id);
    bool unpinChannelMessage(const std::string& channel_id, const std::string& message_id);
    bool setChannelCustomLink(const std::string& channel_id, const std::string& custom_link);
    bool updateChannelName(const std::string& channel_id, const std::string& new_name);
    bool updateChannelDescription(const std::string& channel_id, const std::string& new_description);
    bool setChannelPrivacy(const std::string& channel_id, bool is_private);
    bool setChannelShowAuthor(const std::string& channel_id, bool show_author);
    bool addMessageReaction(const std::string& message_id, const std::string& user_id, const std::string& reaction);
    bool removeMessageReaction(const std::string& message_id, const std::string& user_id, const std::string& reaction);
    std::vector<DatabaseManager::MessageReaction> getMessageReactions(const std::string& message_id);
    bool addMessageView(const std::string& message_id, const std::string& user_id);
    int getMessageViewsCount(const std::string& message_id);
    bool addAllowedReaction(const std::string& channel_id, const std::string& reaction);
    bool removeAllowedReaction(const std::string& channel_id, const std::string& reaction);
    std::vector<std::string> getAllowedReactions(const std::string& channel_id);
    bool createChannelJoinRequest(const std::string& channel_id, const std::string& user_id);
    bool acceptChannelJoinRequest(const std::string& channel_id, const std::string& user_id);
    bool rejectChannelJoinRequest(const std::string& channel_id, const std::string& user_id);
    std::vector<DatabaseManager::ChannelMember> getChannelJoinRequests(const std::string& channel_id);
    bool createChannelChat(const std::string& channel_id, std::string& group_id);
    
    // ========== BOT API OPERATIONS ==========
    struct Bot {
        std::string id;
        std::string owner_id;
        std::string username;
        std::string token;
        std::string first_name;
        std::string description;
        bool is_active;
        std::string created_at;
        std::string webhook_url;
        std::string webhook_secret_token;
        std::vector<std::string> allowed_updates;
    };
    
    struct Update {
        int64_t update_id;
        std::string bot_token;
        std::string update_type; // "message", "callback_query", etc.
        std::string update_data; // JSON string with update data
        int64_t created_at;
    };
    
    struct WebhookInfo {
        std::string url;
        bool has_custom_certificate;
        int pending_update_count;
        std::string last_error_date;
        std::string last_error_message;
        int max_connections;
        std::vector<std::string> allowed_updates;
    };
    
    // Bot management
    bool createBot(const std::string& owner_id, const std::string& username, const std::string& first_name,
                   std::string& bot_id, std::string& token);
    Bot getBotByToken(const std::string& token);
    Bot getBotById(const std::string& bot_id);
    Bot getBotByUsername(const std::string& username);
    bool deleteBot(const std::string& bot_id);
    bool updateBotInfo(const std::string& bot_id, const std::string& first_name = "", 
                      const std::string& description = "");
    
    // Updates
    bool addUpdate(const std::string& bot_token, const std::string& update_type, 
                   const std::string& update_data, int64_t& update_id);
    std::vector<Update> getUpdates(const std::string& bot_token, int64_t offset = 0, 
                                   int limit = 100, int timeout = 0);
    bool confirmUpdate(const std::string& bot_token, int64_t update_id);
    
    // Webhooks
    bool setWebhook(const std::string& bot_token, const std::string& url, 
                   const std::string& secret_token = "", int max_connections = 40,
                   const std::vector<std::string>& allowed_updates = std::vector<std::string>());
    bool deleteWebhook(const std::string& bot_token, bool drop_pending_updates = false);
    WebhookInfo getWebhookInfo(const std::string& bot_token);
    
    // Utility
    std::string generateUUID();
    std::string generateToken();
    std::string getCurrentTimestamp();
    int64_t getCurrentTimestampInt();
    
private:
    InMemoryStorage() = default;
    ~InMemoryStorage() = default;
    InMemoryStorage(const InMemoryStorage&) = delete;
    InMemoryStorage& operator=(const InMemoryStorage&) = delete;
    
    // Thread-safe storage
    mutable std::mutex storage_mutex_;
    
    // User storage
    std::unordered_map<std::string, User> users_; // user_id -> User
    std::unordered_map<std::string, std::string> username_to_id_; // username -> user_id
    std::unordered_map<std::string, int64_t> user_last_activity_; // user_id -> timestamp
    
    // Friend storage
    std::unordered_map<std::string, FriendRequest> friend_requests_; // request_id -> FriendRequest
    std::unordered_map<std::string, std::vector<std::string>> user_friend_requests_; // user_id -> [request_ids]
    std::unordered_map<std::string, std::vector<std::string>> user_friends_; // user_id -> [friend_ids]
    
    // Message storage
    std::unordered_map<std::string, Message> messages_; // message_id -> Message
    std::unordered_map<std::string, std::vector<std::string>> chat_messages_; // "user1_id:user2_id" -> [message_ids]
    std::unordered_map<std::string, int> unread_counts_; // "user_id:sender_id" -> count
    
    // Group storage
    std::unordered_map<std::string, DatabaseManager::Group> groups_; // group_id -> Group
    std::unordered_map<std::string, std::vector<std::string>> user_groups_; // user_id -> [group_ids]
    std::unordered_map<std::string, DatabaseManager::GroupMember> group_members_; // "group_id:user_id" -> GroupMember
    std::unordered_map<std::string, std::vector<std::string>> group_member_list_; // group_id -> [user_ids]
    std::unordered_map<std::string, DatabaseManager::GroupMessage> group_messages_; // message_id -> GroupMessage
    std::unordered_map<std::string, std::vector<std::string>> group_message_list_; // group_id -> [message_ids]
    std::unordered_map<std::string, std::string> group_invite_links_; // invite_link -> group_id
    std::unordered_map<std::string, int64_t> group_invite_expires_; // invite_link -> expire_timestamp
    std::unordered_map<std::string, std::string> pinned_messages_; // "group_id:message_id" -> user_id
    
    // Channel storage
    std::unordered_map<std::string, DatabaseManager::Channel> channels_; // channel_id -> Channel
    std::unordered_map<std::string, std::string> channel_custom_links_; // custom_link -> channel_id
    std::unordered_map<std::string, std::vector<std::string>> user_channels_; // user_id -> [channel_ids]
    std::unordered_map<std::string, DatabaseManager::ChannelMember> channel_members_; // "channel_id:user_id" -> ChannelMember
    std::unordered_map<std::string, std::vector<std::string>> channel_member_list_; // channel_id -> [user_ids]
    std::unordered_map<std::string, DatabaseManager::ChannelMessage> channel_messages_; // message_id -> ChannelMessage
    std::unordered_map<std::string, std::vector<std::string>> channel_message_list_; // channel_id -> [message_ids]
    std::unordered_map<std::string, std::string> pinned_channel_messages_; // "channel_id:message_id" -> user_id
    
    // Reactions and views
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> message_reactions_; // message_id -> [(user_id, reaction)]
    std::unordered_map<std::string, std::vector<std::string>> message_views_; // message_id -> [user_ids]
    std::unordered_map<std::string, std::vector<std::string>> channel_allowed_reactions_; // channel_id -> [reactions]
    std::unordered_map<std::string, std::vector<DatabaseManager::ChannelMember>> channel_join_requests_; // channel_id -> [ChannelMember]
    
    // Bot API storage
    std::unordered_map<std::string, Bot> bots_; // bot_id -> Bot
    std::unordered_map<std::string, std::string> bot_token_to_id_; // token -> bot_id
    std::unordered_map<std::string, std::string> bot_username_to_id_; // username -> bot_id
    std::unordered_map<std::string, std::vector<Update>> bot_updates_; // bot_token -> [Updates]
    std::unordered_map<std::string, int64_t> bot_last_update_id_; // bot_token -> last_update_id
    std::unordered_map<std::string, WebhookInfo> bot_webhooks_; // bot_token -> WebhookInfo
    
    // ID generators
    int64_t user_id_counter_ = 1;
    int64_t message_id_counter_ = 1;
    int64_t group_id_counter_ = 1;
    int64_t channel_id_counter_ = 1;
    int64_t bot_id_counter_ = 1;
    int64_t update_id_counter_ = 1;
    int64_t request_id_counter_ = 1;
    
    // Helper methods
    std::string normalizeChatKey(const std::string& user1_id, const std::string& user2_id);
    bool isNumeric(const std::string& str);
    int64_t stringToInt64(const std::string& str);
    std::string int64ToString(int64_t value);
};

} // namespace xipher

#endif // IN_MEMORY_STORAGE_HPP

