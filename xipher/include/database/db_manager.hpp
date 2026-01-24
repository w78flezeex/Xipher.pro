#ifndef DB_MANAGER_HPP
#define DB_MANAGER_HPP

#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <optional>
#include "db_connection.hpp"

namespace xipher {

struct User {
    std::string id;
    std::string username;
    std::string password_hash;
    std::string created_at;
    bool is_active;
    bool is_admin = false;
    bool is_bot = false;
    bool is_premium = false;
    std::string role;
    std::string totp_secret;
    std::string admin_ip_whitelist;
    std::string premium_plan;
    std::string premium_expires_at;
};

struct PremiumPayment {
    std::string id;
    std::string user_id;
    std::string plan;
    std::string amount;
    std::string label;
    std::string status;
    std::string operation_id;
    std::string payment_type;
    std::string gift_receiver_id;
    std::string created_at;
    std::string paid_at;
};

struct AdminPremiumPayment {
    std::string id;
    std::string label;
    std::string plan;
    std::string amount;
    std::string status;
    std::string operation_id;
    std::string payment_type;
    std::string created_at;
    std::string paid_at;
    std::string user_id;
    std::string username;
};

struct FriendRequest {
    std::string id;
    std::string sender_id;
    std::string receiver_id;
    std::string status;
    std::string created_at;
};

struct Friend {
    std::string id;
    std::string user_id;
    std::string username;
    std::string avatar_url;
    std::string created_at;
};

struct Message {
    std::string id;
    std::string sender_id;
    std::string receiver_id;
    std::string content;
    std::string created_at;
    bool is_read;
    bool is_delivered = false;
    std::string message_type;  // 'text', 'file', 'voice', 'image', 'emoji', 'location', 'live_location'
    std::string file_path;     // Path to file/voice message
    std::string file_name;     // Original filename
    long long file_size;        // File size in bytes
    std::string reply_to_message_id;  // ID of message being replied to
    std::string reply_markup;   // JSON string for inline keyboard buttons
    bool is_pinned = false;
};

struct Report {
    std::string id;
    std::string reporter_id;
    std::string reported_user_id;
    std::string reported_username;
    std::string reporter_username;
    std::string message_id;
    std::string message_type;
    std::string message_content;
    std::string reason;
    std::string comment;
    std::string context_json;
    std::string status;
    std::string resolution_note;
    std::string resolved_by;
    std::string resolved_at;
    std::string created_at;
};

struct AdminUserSummary {
    std::string id;
    std::string username;
    bool is_active = true;
    bool is_admin = false;
    bool is_bot = false;
    std::string role;
    std::string last_login;
    std::string last_activity;
    std::string created_at;
};

struct AdminMessageSummary {
    std::string id;
    std::string sender_id;
    std::string sender_username;
    std::string receiver_id;
    std::string receiver_username;
    std::string content;
    std::string message_type;
    std::string created_at;
};

struct PushTokenInfo {
    std::string device_token;
    std::string platform;
};

struct UserSession {
    std::string token;
    std::string created_at;
    std::string last_seen;
    std::string user_agent;
};

// Advanced chat v2 (unified chats/channels/supergroups)
struct Chat {
    std::string id;
    std::string type;          // chat_type enum
    std::string title;
    std::string username;
    std::string about;
    std::string avatar_url;
    bool is_public = false;
    bool is_verified = false;
    bool sign_messages = true;
    int slow_mode_sec = 0;
    std::string created_by;
    std::string created_at;
};

struct ChatParticipant {
    std::string chat_id;
    std::string user_id;
    std::string status;        // owner/admin/member/...
    std::string admin_role_id;
    std::string joined_at;
};

struct ChatPin {
    std::string chat_type;
    std::string chat_id;
};

struct AdminRole {
    std::string id;
    std::string chat_id;
    std::string title;
    uint64_t perms = 0;
};

struct ChatPermissions {
    bool found = false;
    std::string status;
    uint64_t perms = 0;
};

struct Restriction {
    bool found = false;
    bool can_send_messages = false;
    bool can_send_media = false;
    bool can_add_links = false;
    bool can_invite_users = false;
    std::string until;
};

struct ReactionCount {
    std::string reaction;
    long long count = 0;
};

struct UserProfile {
    std::string user_id;
    std::string first_name;
    std::string last_name;
    std::string bio;
    int birth_day = 0;
    int birth_month = 0;
    int birth_year = 0; // 0 = unknown
    std::string linked_channel_id;
    std::string business_hours_json;
};

struct UserPrivacySettings {
    std::string bio_visibility = "everyone";       // everyone|contacts|nobody
    std::string birth_visibility = "contacts";     // everyone|contacts|nobody
    std::string last_seen_visibility = "contacts"; // everyone|contacts|nobody
    std::string avatar_visibility = "everyone";    // everyone|contacts|nobody
    bool send_read_receipts = true;
};

class DatabaseManager {
public:
    struct Group;
    struct Channel;
    DatabaseManager(const std::string& host = "localhost",
                   const std::string& port = "5432",
                   const std::string& dbname = "xipher",
                   const std::string& user = "xipher",
                   const std::string& password = "xipher");
    
    bool initialize();
    
    // User operations
    bool createUser(const std::string& username, const std::string& password_hash);
    bool createBotUser(const std::string& username, const std::string& password_hash, std::string& out_user_id);
    User getUserByUsername(const std::string& username);
    User getUserById(const std::string& user_id);
    bool usernameExists(const std::string& username);
    bool updateLastLogin(const std::string& user_id);
    bool updateLastActivity(const std::string& user_id);
    bool updateUserAvatar(const std::string& user_id, const std::string& avatar_url);
    bool setUserPremium(const std::string& user_id, bool is_premium, const std::string& plan);
    bool createPremiumPayment(const std::string& user_id, const std::string& plan, const std::string& amount, PremiumPayment& out_payment);
    bool createPremiumGiftPayment(const std::string& user_id, const std::string& receiver_id,
                                  const std::string& plan, const std::string& amount, PremiumPayment& out_payment);
    bool getPremiumPaymentByLabel(const std::string& label, PremiumPayment& out_payment);
    bool markPremiumPaymentPaid(const std::string& label, const std::string& operation_id, const std::string& payment_type);
    bool resetPremiumPaymentStatus(const std::string& label);
    bool hasTrialPayment(const std::string& user_id);
    std::vector<AdminPremiumPayment> listPremiumPayments(const std::string& status, int limit);
    std::string getUserLastActivity(const std::string& user_id);
    bool isUserOnline(const std::string& user_id, int threshold_seconds = 300);  // Default 5 minutes

    // Profile + privacy
    bool upsertUserProfile(const UserProfile& profile);
    bool getUserProfile(const std::string& user_id, UserProfile& out_profile);
    bool setUserLinkedChannel(const std::string& user_id, const std::string& channel_id);
    bool setUserBusinessHours(const std::string& user_id, const std::string& business_hours_json);
    bool upsertUserPrivacy(const std::string& user_id, const UserPrivacySettings& settings);
    bool getUserPrivacy(const std::string& user_id, UserPrivacySettings& out_settings);
    bool getUserPublic(const std::string& user_id, std::string& out_username, std::string& out_avatar_url);
    int countUserChannelsV2(const std::string& user_id);
    int countUserChannelsLegacy(const std::string& user_id);
    int countUserPublicLinksV2(const std::string& user_id);
    int countUserPublicLinksLegacy(const std::string& user_id);
    std::vector<ChatPin> getChatPins(const std::string& user_id);
    bool pinChat(const std::string& user_id, const std::string& chat_type, const std::string& chat_id);
    bool unpinChat(const std::string& user_id, const std::string& chat_type, const std::string& chat_id);
    std::string getChatFoldersJson(const std::string& user_id);
    bool setChatFoldersJson(const std::string& user_id, const std::string& folders_json);

    // Persistent sessions (token -> user_id), survive server restarts
    bool upsertUserSession(const std::string& token,
                           const std::string& user_id,
                           int ttl_seconds = 60 * 60 * 24 * 30); // default 30 days
    std::string getUserIdBySessionToken(const std::string& token);
    bool deleteUserSession(const std::string& token);
    std::vector<UserSession> getUserSessions(const std::string& user_id);
    bool updateUserSessionUserAgent(const std::string& token, const std::string& user_agent);

    // Push tokens
    bool upsertPushToken(const std::string& user_id,
                         const std::string& device_token,
                         const std::string& platform);
    bool deletePushToken(const std::string& user_id, const std::string& device_token);
    std::vector<PushTokenInfo> getPushTokensForUser(const std::string& user_id);
    
    // Friend request operations
    bool createFriendRequest(const std::string& sender_id, const std::string& receiver_id);
    bool acceptFriendRequest(const std::string& request_id);
    bool rejectFriendRequest(const std::string& request_id);
    bool createFriendshipPair(const std::string& user_a, const std::string& user_b);
    std::vector<FriendRequest> getFriendRequests(const std::string& user_id);
    bool areFriends(const std::string& user1_id, const std::string& user2_id);
    std::vector<Friend> getFriends(const std::string& user_id);
    
    // Message operations
    bool sendMessage(const std::string& sender_id, const std::string& receiver_id, const std::string& content,
                     const std::string& message_type = "text", const std::string& file_path = "",
                     const std::string& file_name = "", long long file_size = 0, const std::string& reply_to_message_id = "",
                     const std::string& forwarded_from_user_id = "", const std::string& forwarded_from_username = "",
                     const std::string& forwarded_from_message_id = "", const std::string& reply_markup = "");
    std::vector<Message> getMessages(const std::string& user1_id, const std::string& user2_id, int limit = 50);
    bool pinDirectMessage(const std::string& user1_id, const std::string& user2_id, const std::string& message_id, const std::string& pinned_by);
    bool unpinDirectMessage(const std::string& user1_id, const std::string& user2_id);
    bool getDirectPinnedMessage(const std::string& user1_id, const std::string& user2_id, std::string& out_message_id);
    std::vector<Message> getMessageContext(const std::string& user1_id,
                                           const std::string& user2_id,
                                           const std::string& message_id,
                                           int limit = 7);
    Message getLastMessage(const std::string& user1_id, const std::string& user2_id);
    Message getMessageById(const std::string& message_id);
    bool deleteMessageById(const std::string& message_id);
    int getUnreadCount(const std::string& user_id, const std::string& sender_id);
    bool markMessagesAsRead(const std::string& user_id, const std::string& sender_id);
    bool markMessagesAsDelivered(const std::string& user_id, const std::string& sender_id);
    bool markMessageDeliveredById(const std::string& message_id);
    std::vector<Friend> getChatPartners(const std::string& user_id);  // Получить всех пользователей, с которыми есть переписка
    bool hasSavedMessages(const std::string& user_id);  // Проверить наличие избранных сообщений
    bool setUserActive(const std::string& user_id, bool is_active);
    bool updateUserPasswordHash(const std::string& user_id, const std::string& password_hash);
    bool setUserRole(const std::string& user_id, const std::string& role);
    bool setUserAdminFlag(const std::string& user_id, bool is_admin);
    bool deleteMessagesByUser(const std::string& user_id);
    bool deleteUserSessionsByUser(const std::string& user_id);
    long long countUsers();
    long long countMessagesToday();
    long long countOnlineUsers(int threshold_seconds = 300);
    long long countBannedUsers();
    long long countActiveUsersSince(int seconds);
    long long countGroups();
    long long countChannels();
    long long countBots();
    long long countReportsByStatus(const std::string& status_filter);
    long long countReportsForUserWindow(const std::string& reported_user_id,
                                        const std::string& reason,
                                        int window_minutes);
    std::string getAdminSetting(const std::string& key);
    bool upsertAdminSetting(const std::string& key, const std::string& value);
    std::vector<AdminUserSummary> searchUsers(const std::string& query, int limit);
    std::vector<AdminMessageSummary> searchMessages(const std::string& query, int limit);
    std::vector<Group> searchGroups(const std::string& query, int limit);
    std::vector<Channel> searchChannels(const std::string& query, int limit);
    bool createReport(const std::string& reporter_id,
                      const std::string& reported_user_id,
                      const std::string& message_id,
                      const std::string& message_type,
                      const std::string& message_content,
                      const std::string& reason,
                      const std::string& comment,
                      const std::string& context_json,
                      std::string& out_report_id);
    std::vector<Report> getReportsPaginated(int page,
                                            int page_size,
                                            const std::string& status_filter);
    long long countReports(const std::string& status_filter);
    Report getReportById(const std::string& report_id);
    bool updateReportStatus(const std::string& report_id,
                            const std::string& status,
                            const std::string& resolved_by,
                            const std::string& resolution_note);
    
    // Group operations
    struct Group {
        std::string id;
        std::string name;
        std::string description;
        std::string creator_id;
        std::string invite_link;
        std::string created_at;
        std::string category;  // For public directory
        bool is_public = false;
    };
    
    struct GroupMember {
        std::string id;
        std::string group_id;
        std::string user_id;
        std::string username;
        std::string role; // 'member', 'admin', 'creator'
        std::string permissions; // JSON с правами админа
        bool is_muted;
        bool is_banned;
        std::string joined_at;
    };
    
    struct GroupMessage {
        std::string id;
        std::string group_id;
        std::string sender_id;
        std::string sender_username;
        std::string content;
        std::string message_type;
        std::string file_path;
        std::string file_name;
        long long file_size;
        std::string reply_to_message_id;
        std::string forwarded_from_user_id;
        std::string forwarded_from_username;
        std::string forwarded_from_message_id;
        bool is_pinned;
        std::string created_at;
    };
    
    bool createGroup(const std::string& name, const std::string& description, const std::string& creator_id, std::string& group_id);
    Group getGroupById(const std::string& group_id);
    std::vector<Group> getUserGroups(const std::string& user_id);
    bool addGroupMember(const std::string& group_id, const std::string& user_id, const std::string& role = "member");
    bool removeGroupMember(const std::string& group_id, const std::string& user_id);
    bool updateGroupMemberRole(const std::string& group_id, const std::string& user_id, const std::string& role);
    bool updateAdminPermissions(const std::string& group_id, const std::string& user_id, const std::string& permissions_json);
    bool muteGroupMember(const std::string& group_id, const std::string& user_id, bool muted);
    bool banGroupMember(const std::string& group_id, const std::string& user_id, bool banned, const std::string& until = "");
    std::vector<GroupMember> getGroupMembers(const std::string& group_id);
    GroupMember getGroupMember(const std::string& group_id, const std::string& user_id);
    bool sendGroupMessage(const std::string& group_id, const std::string& sender_id, const std::string& content,
                         const std::string& message_type = "text", const std::string& file_path = "",
                         const std::string& file_name = "", long long file_size = 0,
                         const std::string& reply_to_message_id = "",
                         const std::string& forwarded_from_user_id = "", const std::string& forwarded_from_username = "",
                         const std::string& forwarded_from_message_id = "");
    std::vector<GroupMessage> getGroupMessages(const std::string& group_id, int limit = 50);
    bool pinGroupMessage(const std::string& group_id, const std::string& message_id, const std::string& user_id);
    bool unpinGroupMessage(const std::string& group_id, const std::string& message_id);
    std::string createGroupInviteLink(const std::string& group_id, const std::string& creator_id, int expires_in_seconds = 0);
    // Returns group_id joined, or empty on failure
    std::string joinGroupByInviteLink(const std::string& invite_link, const std::string& user_id);
    bool updateGroupName(const std::string& group_id, const std::string& new_name);
    bool updateGroupDescription(const std::string& group_id, const std::string& new_description);
    
    // Public directory operations
    std::vector<Group> getPublicGroups(const std::string& category = "all", const std::string& search = "", int limit = 50);
    std::vector<Channel> getPublicChannels(const std::string& category = "all", const std::string& search = "", int limit = 50);
    int getGroupMembersCount(const std::string& group_id);
    int getChannelSubscribersCount(const std::string& channel_id);
    bool isGroupMember(const std::string& group_id, const std::string& user_id);
    bool isChannelSubscriber(const std::string& channel_id, const std::string& user_id);
    bool setGroupPublic(const std::string& group_id, bool is_public, const std::string& category = "");
    bool setChannelPublicDirectory(const std::string& channel_id, bool is_public, const std::string& category = "");
    
    // Channel operations
    struct Channel {
        std::string id;
        std::string name;
        std::string description;
        std::string creator_id;
        std::string custom_link;
        std::string avatar_url;
        bool is_private;
        bool show_author;
        std::string created_at;
        std::string category;  // For public directory
        bool is_verified = false;
    };
    
    struct ChannelMember {
        std::string id;
        std::string channel_id;
        std::string user_id;
        std::string username;
        std::string role;
        bool is_banned;
        std::string joined_at;
        uint64_t admin_perms = 0;
        std::string admin_title;
    };
    
    struct ChannelMessage {
        std::string id;
        long long local_id = 0;
        std::string channel_id;
        std::string sender_id;
        std::string sender_username;
        std::string content;
        std::string content_json;
        std::string message_type;
        std::string file_path;
        std::string file_name;
        long long file_size;
        bool is_pinned;
        int views_count;
        std::string created_at;
        bool is_silent = false;
    };
    
    struct MessageReaction {
        std::string reaction;
        int count;
    };
    
    bool createChannel(const std::string& name, const std::string& description, const std::string& creator_id, std::string& channel_id, const std::string& custom_link = "");
    bool checkChannelCustomLinkExists(const std::string& custom_link);
    Channel getChannelById(const std::string& channel_id);
    Channel getChannelByCustomLink(const std::string& custom_link);
    std::vector<Channel> getUserChannels(const std::string& user_id);
    bool addChannelMember(const std::string& channel_id, const std::string& user_id, const std::string& role = "subscriber");
    bool removeChannelMember(const std::string& channel_id, const std::string& user_id);
    bool updateChannelMemberRole(const std::string& channel_id, const std::string& user_id, const std::string& role);
    bool banChannelMember(const std::string& channel_id, const std::string& user_id, bool banned);
    std::vector<ChannelMember> getChannelMembers(const std::string& channel_id);
    ChannelMember getChannelMember(const std::string& channel_id, const std::string& user_id);
    int countChannelSubscribers(const std::string& channel_id);
    int countChannelMembers(const std::string& channel_id);
    bool sendChannelMessage(const std::string& channel_id, const std::string& sender_id, const std::string& content,
                           const std::string& message_type = "text", const std::string& file_path = "",
                           const std::string& file_name = "", long long file_size = 0);
    std::vector<ChannelMessage> getChannelMessages(const std::string& channel_id, int limit = 50);
    bool pinChannelMessage(const std::string& channel_id, const std::string& message_id, const std::string& user_id);
    bool unpinChannelMessage(const std::string& channel_id, const std::string& message_id);
    bool setChannelCustomLink(const std::string& channel_id, const std::string& custom_link);
    bool updateChannelName(const std::string& channel_id, const std::string& new_name);
    bool updateChannelDescription(const std::string& channel_id, const std::string& new_description);
    bool setChannelPrivacy(const std::string& channel_id, bool is_private);
    bool setChannelShowAuthor(const std::string& channel_id, bool show_author);
    bool addMessageReaction(const std::string& message_id, const std::string& user_id, const std::string& reaction);
    bool removeMessageReaction(const std::string& message_id, const std::string& user_id, const std::string& reaction);
    std::vector<MessageReaction> getMessageReactions(const std::string& message_id);
    bool addMessageView(const std::string& message_id, const std::string& user_id);
    int getMessageViewsCount(const std::string& message_id);
    bool addAllowedReaction(const std::string& channel_id, const std::string& reaction);
    bool removeAllowedReaction(const std::string& channel_id, const std::string& reaction);
    std::vector<std::string> getAllowedReactions(const std::string& channel_id);
    bool createChannelJoinRequest(const std::string& channel_id, const std::string& user_id);
    bool acceptChannelJoinRequest(const std::string& channel_id, const std::string& user_id);
    bool rejectChannelJoinRequest(const std::string& channel_id, const std::string& user_id);
    std::vector<ChannelMember> getChannelJoinRequests(const std::string& channel_id);
    bool createChannelChat(const std::string& channel_id, std::string& group_id);
    bool linkChannelChat(const std::string& channel_id, const std::string& group_id);

    // Advanced chats/supergroups/channels (v2, unified model)
    bool createChannelV2(const std::string& title,
                         const std::string& creator_id,
                         std::string& chat_id,
                         bool is_public = false,
                         const std::string& username = "",
                         const std::string& about = "",
                         int slow_mode_sec = 0,
                         bool sign_messages = true);
    bool linkChat(const std::string& channel_id, const std::string& discussion_id);
    bool promoteAdmin(const std::string& chat_id,
                      const std::string& user_id,
                      uint64_t perms,
                      const std::string& title);
    ChatPermissions getChatPermissions(const std::string& chat_id, const std::string& user_id);
    Restriction getParticipantRestriction(const std::string& chat_id, const std::string& user_id);
    Chat getChatV2(const std::string& chat_id);
    Chat getChatByUsernameV2(const std::string& username);
    bool sendChannelMessageWithDiscussion(const std::string& channel_id,
                                          const std::string& sender_id,
                                          const std::string& content_json,
                                          const std::string& content_type,
                                          const std::string& replied_to_global_id,
                                          bool is_silent,
                                          std::string* out_channel_message_id = nullptr,
                                          std::string* out_discussion_message_id = nullptr);

    // Reactions (v2 chat_messages)
    bool addMessageReactionV2(const std::string& message_id, const std::string& user_id, const std::string& reaction);
    bool removeMessageReactionV2(const std::string& message_id, const std::string& user_id, const std::string& reaction);
    std::vector<ReactionCount> getMessageReactionCountsV2(const std::string& message_id);
    bool pinChatMessageV2(const std::string& chat_id, const std::string& message_id, const std::string& pinned_by);
    bool unpinChatMessageV2(const std::string& chat_id);
    bool getChatMessageMetaV2(const std::string& message_id, std::string& out_chat_id, std::string& out_sender_id);
    bool deleteChatMessageV2(const std::string& chat_id, const std::string& message_id);
    bool getChannelMessageMetaLegacy(const std::string& message_id, std::string& out_channel_id, std::string& out_sender_id);
    bool deleteChannelMessageLegacy(const std::string& channel_id, const std::string& message_id);
    bool getGroupMessageMeta(const std::string& message_id, std::string& out_group_id, std::string& out_sender_id);
    bool deleteGroupMessage(const std::string& group_id, const std::string& message_id);
    
    // Group management
    bool deleteGroup(const std::string& group_id);
    std::vector<GroupMessage> searchGroupMessages(const std::string& group_id, const std::string& query, int limit = 50);
    bool updateGroupPermission(const std::string& group_id, const std::string& permission, bool enabled);
    
    std::vector<ChannelMessage> getChannelMessagesV2(const std::string& chat_id,
                                                     long long offset_local = 0,
                                                     int limit = 50);
    bool upsertChannelReadState(const std::string& chat_id,
                                const std::string& user_id,
                                long long max_read_local);
    long long getChannelUnreadCount(const std::string& chat_id,
                                    const std::string& user_id);
    bool createChannelInviteLink(const std::string& channel_id,
                                 const std::string& creator_id,
                                 int expire_seconds,
                                 int usage_limit,
                                 std::string& out_token);
    bool revokeChannelInviteLink(const std::string& token);
    bool joinChannelByInviteToken(const std::string& token,
                                  const std::string& user_id,
                                  std::string& out_channel_id,
                                  bool auto_join = true,
                                  bool* out_is_v2 = nullptr);
    std::string getChannelIdByInviteToken(const std::string& token);
    std::vector<std::string> getChannelSubscriberIds(const std::string& chat_id);
    int countChannelSubscribersV2(const std::string& chat_id);
    int countChannelMembersV2(const std::string& chat_id);

    // v2 chat updates (channels/supergroups)
    bool updateChatTitleV2(const std::string& chat_id, const std::string& title);
    bool updateChatAboutV2(const std::string& chat_id, const std::string& about);
    bool updateChatUsernameV2(const std::string& chat_id, const std::string& username);
    bool updateChatIsPublicV2(const std::string& chat_id, bool is_public);
    bool updateChatSignMessagesV2(const std::string& chat_id, bool sign_messages);
    bool updateChatAvatarV2(const std::string& chat_id, const std::string& avatar_url);

    // v2 membership helpers
    bool upsertChatMemberV2(const std::string& chat_id, const std::string& user_id);
    bool leaveChatV2(const std::string& chat_id, const std::string& user_id);
    bool demoteChatAdminV2(const std::string& chat_id, const std::string& user_id);
    ChatParticipant getChatParticipantV2(const std::string& chat_id, const std::string& user_id);
    bool updateChatParticipantStatusV2(const std::string& chat_id, const std::string& user_id, const std::string& status);

    // v2 join requests / reactions
    bool createChatJoinRequest(const std::string& chat_id, const std::string& user_id);
    bool acceptChatJoinRequest(const std::string& chat_id, const std::string& user_id);
    bool rejectChatJoinRequest(const std::string& chat_id, const std::string& user_id);
    std::vector<ChannelMember> getChatJoinRequests(const std::string& chat_id);
    bool addChatAllowedReaction(const std::string& chat_id, const std::string& reaction);
    bool removeChatAllowedReaction(const std::string& chat_id, const std::string& reaction);
    std::vector<std::string> getChatAllowedReactions(const std::string& chat_id);

    // v2 channels list/members (compat with legacy Channel/ChannelMember shapes)
    std::vector<Channel> getUserChannelsV2(const std::string& user_id);
    std::vector<ChannelMember> getChannelMembersV2(const std::string& channel_id);
    bool deleteChatV2(const std::string& chat_id);
    bool updateChannelAvatar(const std::string& channel_id, const std::string& avatar_url);
    bool deleteChannelLegacy(const std::string& channel_id);

    // Polls (supports chat, channel, group messages)
    bool createPollV2(const std::string& message_id,
                      const std::string& message_type,  // "chat", "channel", or "group"
                      const std::string& question,
                      bool is_anonymous,
                      bool allows_multiple,
                      const std::vector<std::string>& options,
                      const std::string& closes_at_iso = "");
    bool votePollV2(const std::string& poll_id,
                    const std::string& option_id,
                    const std::string& user_id);
    struct PollOption {
        std::string id;
        std::string option_text;
        long long vote_count;
    };
    struct Poll {
        std::string id;
        std::string message_id;
        std::string message_type;
        std::string question;
        bool is_anonymous;
        bool allows_multiple;
        std::string closes_at;
        std::vector<PollOption> options;
    };
    Poll getPollByMessage(const std::string& message_type, const std::string& message_id);
    
    // Marketplace operations
    struct MarketProduct {
        std::string id;
        std::string owner_chat_id;
        std::string owner_type;
        std::string title;
        std::string description;
        double price_amount;
        std::string price_currency;
        std::string product_type;
        std::string image_path;
        bool is_active;
        int stock_quantity;
        std::string created_at;
        std::string created_by;
    };
    
    struct ProductKey {
        std::string id;
        std::string product_id;
        std::string key_value;
        bool is_used;
        std::string used_by;
        std::string used_at;
    };
    
    struct ProductPurchase {
        std::string id;
        std::string product_id;
        std::string buyer_id;
        std::string seller_id;
        double price_amount;
        std::string price_currency;
        double platform_fee;
        std::string status;
        std::string invoice_id;
        std::string key_id;
        std::string created_at;
    };
    
    bool createMarketProduct(const std::string& owner_chat_id, const std::string& owner_type,
                            const std::string& title, const std::string& description,
                            double price_amount, const std::string& price_currency,
                            const std::string& product_type, const std::string& image_path,
                            int stock_quantity, const std::string& created_by, std::string& product_id);
    bool updateMarketProduct(const std::string& product_id, const std::string& title,
                            const std::string& description, double price_amount,
                            const std::string& price_currency, const std::string& image_path,
                            int stock_quantity, bool is_active);
    bool deleteMarketProduct(const std::string& product_id);
    std::vector<MarketProduct> getMarketProducts(const std::string& owner_chat_id, const std::string& owner_type);
    MarketProduct getMarketProductById(const std::string& product_id);
    bool addProductKey(const std::string& product_id, const std::string& key_value);
    bool purchaseProduct(const std::string& product_id, const std::string& buyer_id,
                         const std::string& invoice_id, std::string& purchase_id, std::string& key_id);
    std::vector<ProductPurchase> getUserPurchases(const std::string& user_id);
    
    // Integration operations
    struct IntegrationBinding {
        std::string id;
        std::string chat_id;
        std::string chat_type;
        std::string service_name;
        std::string config_json;
        bool is_active;
        std::string created_at;
        std::string created_by;
    };
    
    bool createIntegrationBinding(const std::string& chat_id, const std::string& chat_type,
                                 const std::string& service_name, const std::string& external_token,
                                 const std::string& config_json, const std::string& created_by, std::string& binding_id);
    bool updateIntegrationBinding(const std::string& binding_id, const std::string& config_json, bool is_active);
    bool deleteIntegrationBinding(const std::string& binding_id);
    std::vector<IntegrationBinding> getIntegrationBindings(const std::string& chat_id, const std::string& chat_type);
    IntegrationBinding getIntegrationBindingById(const std::string& binding_id);
    
    // Bot Builder operations
    struct BotBuilderBot {
        std::string id;
        std::string user_id;
        std::string bot_user_id;
        std::string bot_token;
        std::string bot_username;
        std::string bot_name;
        std::string bot_description;
        std::string bot_avatar_url;
        std::string flow_json;
        bool is_active;
        std::string created_at;
        std::string deployed_at;
    };
    
    bool createBotBuilderBot(const std::string& user_id, const std::string& bot_user_id, const std::string& bot_token,
                            const std::string& bot_username, const std::string& bot_name,
                            const std::string& flow_json, std::string& bot_id);
    bool updateBotBuilderBot(const std::string& bot_id, const std::string& flow_json, bool is_active);
    bool updateBotBuilderProfile(const std::string& bot_id, const std::string& bot_name, const std::string& bot_description);
    bool updateBotBuilderAvatarUrl(const std::string& bot_id, const std::string& bot_avatar_url);
    bool deleteBotBuilderBot(const std::string& bot_id);
    std::vector<BotBuilderBot> getUserBots(const std::string& user_id);
    BotBuilderBot getBotBuilderBotByToken(const std::string& bot_token);
    BotBuilderBot getBotBuilderBotByUsername(const std::string& bot_username);
    BotBuilderBot getBotBuilderBotById(const std::string& bot_id);
    BotBuilderBot getBotBuilderBotByUserId(const std::string& bot_user_id);
    bool logBotExecution(const std::string& bot_id, const std::string& update_id,
                        const std::string& user_id, const std::string& chat_id,
                        int node_id, const std::string& node_type,
                        const std::string& execution_result, int execution_time_ms,
                        const std::string& error_message);

    // Bot runtime helpers (notes + reminders)
    bool upsertBotNote(const std::string& bot_user_id,
                       const std::string& scope_type,
                       const std::string& scope_id,
                       const std::string& note_key,
                       const std::string& note_value,
                       const std::string& created_by_user_id);
    bool getBotNote(const std::string& bot_user_id,
                    const std::string& scope_type,
                    const std::string& scope_id,
                    const std::string& note_key,
                    std::string& out_note_value);
    bool listBotNotes(const std::string& bot_user_id,
                      const std::string& scope_type,
                      const std::string& scope_id,
                      std::vector<std::string>& out_keys);
    bool deleteBotNote(const std::string& bot_user_id,
                       const std::string& scope_type,
                       const std::string& scope_id,
                       const std::string& note_key);

    bool createBotReminder(const std::string& bot_user_id,
                           const std::string& scope_type,
                           const std::string& scope_id,
                           const std::string& target_user_id,
                           const std::string& reminder_text,
                           int delay_seconds,
                           std::string& out_reminder_id);

    struct BotReminderDue {
        std::string id;
        std::string bot_user_id;
        std::string scope_type;   // direct|group
        std::string scope_id;     // user_id or group_id
        std::string target_user_id;
        std::string text;
    };
    std::vector<BotReminderDue> claimDueBotReminders(int limit = 50);

    // Group bot runtime helper: list bot users (bot_user_id) installed as members
    std::vector<std::string> getGroupBotUserIds(const std::string& group_id);

    // Bot developers (collaboration)
    struct BotDeveloper {
        std::string developer_user_id;
        std::string developer_username;
        std::string added_by_user_id;
        std::string added_at;
    };
    
    bool addBotDeveloper(const std::string& bot_id, const std::string& developer_user_id, const std::string& added_by_user_id);
    bool removeBotDeveloper(const std::string& bot_id, const std::string& developer_user_id);
    std::vector<BotDeveloper> getBotDevelopers(const std::string& bot_id);
    bool isBotDeveloper(const std::string& bot_id, const std::string& user_id);
    bool canEditBot(const std::string& bot_id, const std::string& user_id);  // Owner or developer

    // Bot project files (for IDE)
    struct BotFile {
        std::string id;
        std::string bot_id;
        std::string file_path;
        std::string file_content;
        std::string created_at;
        std::string updated_at;
    };
    
    bool upsertBotFile(const std::string& bot_id, const std::string& file_path, const std::string& file_content);
    bool getBotFile(const std::string& bot_id, const std::string& file_path, std::string& out_content);
    bool deleteBotFile(const std::string& bot_id, const std::string& file_path);
    std::vector<BotFile> listBotFiles(const std::string& bot_id);

    // Bot scripts (Python)
    bool getBotBuilderScriptByBotUserId(const std::string& bot_user_id,
                                        std::string& out_lang,
                                        bool& out_enabled,
                                        std::string& out_code);
    bool getBotBuilderScriptById(const std::string& bot_id,
                                 std::string& out_lang,
                                 bool& out_enabled,
                                 std::string& out_code);
    bool updateBotBuilderScript(const std::string& bot_id,
                                const std::string& lang,
                                bool enabled,
                                const std::string& code);
    
    // Event Router operations
    struct TriggerRule {
        std::string id;
        std::string chat_id;
        std::string chat_type;
        std::string rule_name;
        std::string trigger_conditions;
        std::string actions;
        bool is_active;
        int rate_limit_per_second;
        std::string created_at;
        std::string created_by;
    };
    
    struct EventWebhook {
        std::string id;
        std::string trigger_rule_id;
        std::string webhook_url;
        std::string secret_token;
        std::string http_method;
        std::string headers_json;
        bool is_active;
        std::string last_sent_at;
        int last_status_code;
    };
    
    bool createTriggerRule(const std::string& chat_id, const std::string& chat_type,
                         const std::string& rule_name, const std::string& trigger_conditions,
                         const std::string& actions, int rate_limit_per_second,
                         const std::string& created_by, std::string& rule_id);
    bool updateTriggerRule(const std::string& rule_id, const std::string& trigger_conditions,
                          const std::string& actions, bool is_active, int rate_limit_per_second);
    bool deleteTriggerRule(const std::string& rule_id);
    std::vector<TriggerRule> getTriggerRules(const std::string& chat_id, const std::string& chat_type);
    std::optional<TriggerRule> getTriggerRuleById(const std::string& rule_id);
    bool createEventWebhook(const std::string& trigger_rule_id, const std::string& webhook_url,
                           const std::string& secret_token, const std::string& http_method,
                           const std::string& headers_json, std::string& webhook_id);
    bool logTriggerExecution(const std::string& trigger_rule_id, const std::string& event_type,
                            const std::string& event_data, const std::string& execution_result,
                            const std::string& status, const std::string& error_message);
    
    // Group Topics (Forum mode)
    struct GroupTopic {
        std::string id;
        std::string group_id;
        std::string name;
        std::string icon_emoji;
        std::string icon_color;
        bool is_general = false;
        bool is_closed = false;
        bool is_hidden = false;  // For hiding General topic
        int pinned_order = 0;
        std::string last_message_at;
        std::string last_message_content;
        std::string last_message_sender;
        std::string last_message_sender_id;
        int message_count = 0;
        int unread_count = 0;
        std::string created_by;
        std::string created_at;
    };
    
    bool setGroupForumMode(const std::string& group_id, bool enabled);
    bool isGroupForumMode(const std::string& group_id);
    bool createGroupTopic(const std::string& group_id, const std::string& name, 
                         const std::string& icon_emoji, const std::string& icon_color,
                         const std::string& created_by, bool is_general, std::string& topic_id);
    bool updateGroupTopic(const std::string& topic_id, const std::string& name,
                         const std::string& icon_emoji, const std::string& icon_color, bool is_closed);
    bool deleteGroupTopic(const std::string& topic_id);
    bool pinGroupTopic(const std::string& topic_id, int order);
    bool unpinGroupTopic(const std::string& topic_id);
    std::vector<GroupTopic> getGroupTopics(const std::string& group_id);
    GroupTopic getGroupTopicById(const std::string& topic_id);
    bool sendGroupMessageToTopic(const std::string& group_id, const std::string& topic_id, 
                                const std::string& sender_id, const std::string& content,
                                const std::string& message_type = "text", const std::string& file_path = "",
                                const std::string& file_name = "", long long file_size = 0,
                                const std::string& reply_to_message_id = "");
    std::vector<GroupMessage> getTopicMessages(const std::string& topic_id, int limit = 50);
    bool updateTopicMessageCount(const std::string& topic_id);
    
    // Telegram-like topic functions
    bool setTopicHidden(const std::string& topic_id, bool is_hidden);
    bool setTopicClosed(const std::string& topic_id, bool is_closed);
    bool reorderPinnedTopics(const std::string& group_id, const std::vector<std::string>& topic_ids);
    std::vector<GroupTopic> searchTopics(const std::string& group_id, const std::string& query);
    bool setTopicNotifications(const std::string& topic_id, const std::string& user_id, const std::string& mode);
    
    // Profile stats for user-to-user conversations
    struct UserChatStats {
        long long photo_count = 0;
        long long video_count = 0;
        long long file_count = 0;
        long long audio_count = 0;
        long long voice_count = 0;
        long long link_count = 0;
        long long gif_count = 0;
        long long saved_count = 0;  // favorites
    };
    UserChatStats getUserChatStats(const std::string& viewer_id, const std::string& target_id);
    
    // Common groups between two users
    std::vector<Group> getCommonGroups(const std::string& user1_id, const std::string& user2_id, int limit = 10);
    
    // Raw database access for custom queries
    DatabaseConnection* getDb() { return db_.get(); }
    
private:
    std::unique_ptr<DatabaseConnection> db_;
    
    void initializeSchema();
    void prepareStatements();
};

} // namespace xipher

#endif // DB_MANAGER_HPP
