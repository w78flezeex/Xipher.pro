#include "../include/storage/in_memory_storage.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <random>
#include <algorithm>
#include <map>
#include <unordered_map>
#include <openssl/sha.h>
#include <openssl/rand.h>

namespace xipher {

InMemoryStorage& InMemoryStorage::getInstance() {
    static InMemoryStorage instance;
    return instance;
}

bool InMemoryStorage::initialize() {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    Logger::getInstance().info("InMemoryStorage initialized successfully");
    return true;
}

// ========== UTILITY METHODS ==========

std::string InMemoryStorage::generateUUID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);
    
    std::ostringstream oss;
    oss << std::hex;
    for (int i = 0; i < 32; i++) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            oss << "-";
        }
        if (i == 12) {
            oss << "4"; // Version 4 UUID
        } else if (i == 16) {
            oss << dis2(gen); // Variant
        } else {
            oss << dis(gen);
        }
    }
    return oss.str();
}

std::string InMemoryStorage::generateToken() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 61);
    
    const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::ostringstream oss;
    
    // Format: user_id:random_token (like Telegram Bot API)
    oss << (bot_id_counter_++) << ":";
    
    for (int i = 0; i < 35; i++) {
        oss << chars[dis(gen)];
    }
    
    return oss.str();
}

std::string InMemoryStorage::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::gmtime(&time);
    
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

int64_t InMemoryStorage::getCurrentTimestampInt() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string InMemoryStorage::normalizeChatKey(const std::string& user1_id, const std::string& user2_id) {
    // Normalize chat key for bidirectional chats
    if (user1_id < user2_id) {
        return user1_id + ":" + user2_id;
    }
    return user2_id + ":" + user1_id;
}

bool InMemoryStorage::isNumeric(const std::string& str) {
    return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

int64_t InMemoryStorage::stringToInt64(const std::string& str) {
    try {
        return std::stoll(str);
    } catch (...) {
        return 0;
    }
}

std::string InMemoryStorage::int64ToString(int64_t value) {
    return std::to_string(value);
}

// ========== USER OPERATIONS ==========

bool InMemoryStorage::createUser(const std::string& username, const std::string& password_hash, std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Check if username already exists
    if (username_to_id_.find(username) != username_to_id_.end()) {
        return false;
    }
    
    user_id = generateUUID();
    
    User user;
    user.id = user_id;
    user.username = username;
    user.password_hash = password_hash;
    user.created_at = getCurrentTimestamp();
    user.is_active = true;
    
    users_[user_id] = user;
    username_to_id_[username] = user_id;
    user_last_activity_[user_id] = getCurrentTimestampInt();
    
    return true;
}

User InMemoryStorage::getUserByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = username_to_id_.find(username);
    if (it == username_to_id_.end()) {
        return User{}; // Return empty user
    }
    
    auto user_it = users_.find(it->second);
    if (user_it == users_.end()) {
        return User{};
    }
    
    return user_it->second;
}

User InMemoryStorage::getUserById(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = users_.find(user_id);
    if (it == users_.end()) {
        return User{};
    }
    
    return it->second;
}

bool InMemoryStorage::usernameExists(const std::string& username) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    return username_to_id_.find(username) != username_to_id_.end();
}

bool InMemoryStorage::updateLastLogin(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (users_.find(user_id) == users_.end()) {
        return false;
    }
    
    return updateLastActivity(user_id);
}

bool InMemoryStorage::updateLastActivity(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (users_.find(user_id) == users_.end()) {
        return false;
    }
    
    user_last_activity_[user_id] = getCurrentTimestampInt();
    return true;
}

std::string InMemoryStorage::getUserLastActivity(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = user_last_activity_.find(user_id);
    if (it == user_last_activity_.end()) {
        return "";
    }
    
    auto time = std::chrono::system_clock::from_time_t(it->second);
    auto tm = std::chrono::system_clock::to_time_t(time);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&tm), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

bool InMemoryStorage::isUserOnline(const std::string& user_id, int threshold_seconds) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = user_last_activity_.find(user_id);
    if (it == user_last_activity_.end()) {
        return false;
    }
    
    int64_t current_time = getCurrentTimestampInt();
    int64_t last_activity = it->second;
    
    return (current_time - last_activity) < threshold_seconds;
}

// ========== FRIEND OPERATIONS ==========

bool InMemoryStorage::createFriendRequest(const std::string& sender_id, const std::string& receiver_id, std::string& request_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Check if already friends
    if (areFriends(sender_id, receiver_id)) {
        return false;
    }
    
    // Check if request already exists
    for (const auto& req : friend_requests_) {
        if ((req.second.sender_id == sender_id && req.second.receiver_id == receiver_id && req.second.status == "pending") ||
            (req.second.sender_id == receiver_id && req.second.receiver_id == sender_id && req.second.status == "pending")) {
            return false;
        }
    }
    
    request_id = generateUUID();
    
    FriendRequest request;
    request.id = request_id;
    request.sender_id = sender_id;
    request.receiver_id = receiver_id;
    request.status = "pending";
    request.created_at = getCurrentTimestamp();
    
    friend_requests_[request_id] = request;
    user_friend_requests_[receiver_id].push_back(request_id);
    
    return true;
}

bool InMemoryStorage::acceptFriendRequest(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = friend_requests_.find(request_id);
    if (it == friend_requests_.end() || it->second.status != "pending") {
        return false;
    }
    
    FriendRequest& request = it->second;
    request.status = "accepted";
    
    // Add to friends list
    user_friends_[request.sender_id].push_back(request.receiver_id);
    user_friends_[request.receiver_id].push_back(request.sender_id);
    
    return true;
}

bool InMemoryStorage::rejectFriendRequest(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = friend_requests_.find(request_id);
    if (it == friend_requests_.end()) {
        return false;
    }
    
    it->second.status = "rejected";
    return true;
}

std::vector<FriendRequest> InMemoryStorage::getFriendRequests(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<FriendRequest> requests;
    
    for (const auto& req_pair : friend_requests_) {
        const FriendRequest& req = req_pair.second;
        if (req.receiver_id == user_id && req.status == "pending") {
            requests.push_back(req);
        }
    }
    
    return requests;
}

bool InMemoryStorage::areFriends(const std::string& user1_id, const std::string& user2_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = user_friends_.find(user1_id);
    if (it == user_friends_.end()) {
        return false;
    }
    
    const auto& friends = it->second;
    return std::find(friends.begin(), friends.end(), user2_id) != friends.end();
}

std::vector<Friend> InMemoryStorage::getFriends(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<Friend> friends;
    
    auto it = user_friends_.find(user_id);
    if (it == user_friends_.end()) {
        return friends;
    }
    
    for (const auto& friend_id : it->second) {
        auto user_it = users_.find(friend_id);
        if (user_it != users_.end()) {
            Friend f;
            f.id = friend_id;
            f.user_id = friend_id;
            f.username = user_it->second.username;
            f.created_at = getCurrentTimestamp(); // TODO: Store actual friendship creation time
            friends.push_back(f);
        }
    }
    
    return friends;
}

// ========== MESSAGE OPERATIONS ==========

bool InMemoryStorage::sendMessage(const std::string& sender_id, const std::string& receiver_id, 
                                  const std::string& content, const std::string& message_type,
                                  const std::string& file_path, const std::string& file_name,
                                  long long file_size, const std::string& reply_to_message_id,
                                  const std::string& forwarded_from_user_id,
                                  const std::string& forwarded_from_username,
                                  const std::string& forwarded_from_message_id,
                                  std::string* message_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::string msg_id = generateUUID();
    if (message_id) {
        *message_id = msg_id;
    }
    
    Message msg;
    msg.id = msg_id;
    msg.sender_id = sender_id;
    msg.receiver_id = receiver_id;
    msg.content = content;
    msg.created_at = getCurrentTimestamp();
    msg.is_read = false;
    msg.is_delivered = false;
    msg.message_type = message_type;
    msg.file_path = file_path;
    msg.file_name = file_name;
    msg.file_size = file_size;
    msg.reply_to_message_id = reply_to_message_id;
    
    messages_[msg_id] = msg;
    
    std::string chat_key = normalizeChatKey(sender_id, receiver_id);
    chat_messages_[chat_key].push_back(msg_id);
    
    // Update unread count
    std::string unread_key = receiver_id + ":" + sender_id;
    unread_counts_[unread_key]++;
    
    return true;
}

std::vector<Message> InMemoryStorage::getMessages(const std::string& user1_id, const std::string& user2_id, int limit) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<Message> result;
    std::string chat_key = normalizeChatKey(user1_id, user2_id);
    
    auto it = chat_messages_.find(chat_key);
    if (it == chat_messages_.end()) {
        return result;
    }
    
    // Get messages in reverse order (newest first), then reverse for chronological order
    const auto& message_ids = it->second;
    int count = std::min(limit, static_cast<int>(message_ids.size()));
    
    for (int i = message_ids.size() - count; i < static_cast<int>(message_ids.size()); i++) {
        auto msg_it = messages_.find(message_ids[i]);
        if (msg_it != messages_.end()) {
            result.push_back(msg_it->second);
        }
    }
    
    return result;
}

Message InMemoryStorage::getLastMessage(const std::string& user1_id, const std::string& user2_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::string chat_key = normalizeChatKey(user1_id, user2_id);
    
    auto it = chat_messages_.find(chat_key);
    if (it == chat_messages_.end() || it->second.empty()) {
        return Message{};
    }
    
    const auto& message_ids = it->second;
    std::string last_message_id = message_ids.back();
    
    auto msg_it = messages_.find(last_message_id);
    if (msg_it != messages_.end()) {
        return msg_it->second;
    }
    
    return Message{};
}

Message InMemoryStorage::getMessageById(const std::string& message_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = messages_.find(message_id);
    if (it != messages_.end()) {
        return it->second;
    }
    
    return Message{};
}

bool InMemoryStorage::editMessage(const std::string& message_id, const std::string& new_content) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = messages_.find(message_id);
    if (it != messages_.end()) {
        it->second.content = new_content;
        return true;
    }
    
    return false;
}

bool InMemoryStorage::deleteMessage(const std::string& message_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto msg_it = messages_.find(message_id);
    if (msg_it == messages_.end()) {
        return false;
    }
    
    // Remove from chat messages list
    std::string sender_id = msg_it->second.sender_id;
    std::string receiver_id = msg_it->second.receiver_id;
    std::string chat_key = normalizeChatKey(sender_id, receiver_id);
    
    auto chat_it = chat_messages_.find(chat_key);
    if (chat_it != chat_messages_.end()) {
        auto& message_ids = chat_it->second;
        message_ids.erase(
            std::remove(message_ids.begin(), message_ids.end(), message_id),
            message_ids.end()
        );
    }
    
    // Remove message
    messages_.erase(msg_it);
    
    return true;
}

int InMemoryStorage::getUnreadCount(const std::string& user_id, const std::string& sender_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::string unread_key = user_id + ":" + sender_id;
    auto it = unread_counts_.find(unread_key);
    
    if (it == unread_counts_.end()) {
        return 0;
    }
    
    return it->second;
}

bool InMemoryStorage::markMessagesAsRead(const std::string& user_id, const std::string& sender_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::string chat_key = normalizeChatKey(user_id, sender_id);
    auto it = chat_messages_.find(chat_key);
    
    if (it == chat_messages_.end()) {
        return false;
    }
    
    // Mark all unread messages as read
    for (const auto& message_id : it->second) {
        auto msg_it = messages_.find(message_id);
        if (msg_it != messages_.end() && msg_it->second.receiver_id == user_id && !msg_it->second.is_read) {
            msg_it->second.is_read = true;
        }
    }
    
    // Reset unread count
    std::string unread_key = user_id + ":" + sender_id;
    unread_counts_[unread_key] = 0;
    
    return true;
}

std::vector<Friend> InMemoryStorage::getChatPartners(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<Friend> partners;
    std::unordered_map<std::string, bool> seen;
    
    // Find all unique chat partners
    for (const auto& chat_pair : chat_messages_) {
        const std::string& chat_key = chat_pair.first;
        size_t colon_pos = chat_key.find(':');
        if (colon_pos == std::string::npos) continue;
        
        std::string user1 = chat_key.substr(0, colon_pos);
        std::string user2 = chat_key.substr(colon_pos + 1);
        
        std::string partner_id;
        if (user1 == user_id) {
            partner_id = user2;
        } else if (user2 == user_id) {
            partner_id = user1;
        } else {
            continue;
        }
        
        if (!seen[partner_id]) {
            seen[partner_id] = true;
            auto user_it = users_.find(partner_id);
            if (user_it != users_.end()) {
                Friend f;
                f.id = partner_id;
                f.user_id = partner_id;
                f.username = user_it->second.username;
                f.created_at = getCurrentTimestamp();
                partners.push_back(f);
            }
        }
    }
    
    return partners;
}

// ========== GROUP OPERATIONS ==========

bool InMemoryStorage::createGroup(const std::string& name, const std::string& description, 
                                  const std::string& creator_id, std::string& group_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    group_id = generateUUID();
    
    DatabaseManager::Group group;
    group.id = group_id;
    group.name = name;
    group.description = description;
    group.creator_id = creator_id;
    group.invite_link = "";
    group.created_at = getCurrentTimestamp();
    
    groups_[group_id] = group;
    user_groups_[creator_id].push_back(group_id);
    
    // Add creator as member with role "creator"
    DatabaseManager::GroupMember member;
    member.id = generateUUID();
    member.group_id = group_id;
    member.user_id = creator_id;
    member.username = getUserById(creator_id).username;
    member.role = "creator";
    member.is_muted = false;
    member.is_banned = false;
    member.joined_at = getCurrentTimestamp();
    
    group_members_[group_id + "_" + creator_id] = member;
    group_member_list_[group_id].push_back(creator_id);
    
    return true;
}

DatabaseManager::Group InMemoryStorage::getGroupById(const std::string& group_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = groups_.find(group_id);
    if (it != groups_.end()) {
        return it->second;
    }
    
    return DatabaseManager::Group{};
}

std::vector<DatabaseManager::Group> InMemoryStorage::getUserGroups(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<DatabaseManager::Group> result;
    auto it = user_groups_.find(user_id);
    
    if (it != user_groups_.end()) {
        for (const auto& group_id : it->second) {
            auto group_it = groups_.find(group_id);
            if (group_it != groups_.end()) {
                result.push_back(group_it->second);
            }
        }
    }
    
    return result;
}

bool InMemoryStorage::addGroupMember(const std::string& group_id, const std::string& user_id, const std::string& role) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (groups_.find(group_id) == groups_.end()) {
        return false;
    }
    
    std::string key = group_id + "_" + user_id;
    if (group_members_.find(key) != group_members_.end()) {
        return false; // Already a member
    }
    
    User user = getUserById(user_id);
    if (user.id.empty()) {
        return false;
    }
    
    DatabaseManager::GroupMember member;
    member.id = generateUUID();
    member.group_id = group_id;
    member.user_id = user_id;
    member.username = user.username;
    member.role = role;
    member.is_muted = false;
    member.is_banned = false;
    member.joined_at = getCurrentTimestamp();
    
    group_members_[key] = member;
    group_member_list_[group_id].push_back(user_id);
    user_groups_[user_id].push_back(group_id);
    
    return true;
}

bool InMemoryStorage::removeGroupMember(const std::string& group_id, const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::string key = group_id + "_" + user_id;
    auto it = group_members_.find(key);
    
    if (it == group_members_.end()) {
        return false;
    }
    
    group_members_.erase(it);
    
    // Remove from group member list
    auto& member_list = group_member_list_[group_id];
    member_list.erase(std::remove(member_list.begin(), member_list.end(), user_id), member_list.end());
    
    // Remove from user groups
    auto& user_groups_list = user_groups_[user_id];
    user_groups_list.erase(std::remove(user_groups_list.begin(), user_groups_list.end(), group_id), user_groups_list.end());
    
    return true;
}

bool InMemoryStorage::updateGroupMemberRole(const std::string& group_id, const std::string& user_id, const std::string& role) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::string key = group_id + "_" + user_id;
    auto it = group_members_.find(key);
    
    if (it != group_members_.end()) {
        it->second.role = role;
        return true;
    }
    
    return false;
}

bool InMemoryStorage::muteGroupMember(const std::string& group_id, const std::string& user_id, bool muted) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::string key = group_id + "_" + user_id;
    auto it = group_members_.find(key);
    
    if (it != group_members_.end()) {
        it->second.is_muted = muted;
        return true;
    }
    
    return false;
}

bool InMemoryStorage::banGroupMember(const std::string& group_id, const std::string& user_id, bool banned, const std::string& until) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::string key = group_id + "_" + user_id;
    auto it = group_members_.find(key);
    
    if (it != group_members_.end()) {
        it->second.is_banned = banned;
        return true;
    }
    
    return false;
}

std::vector<DatabaseManager::GroupMember> InMemoryStorage::getGroupMembers(const std::string& group_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<DatabaseManager::GroupMember> result;
    auto it = group_member_list_.find(group_id);
    
    if (it != group_member_list_.end()) {
        for (const auto& user_id : it->second) {
            std::string key = group_id + "_" + user_id;
            auto member_it = group_members_.find(key);
            if (member_it != group_members_.end()) {
                result.push_back(member_it->second);
            }
        }
    }
    
    return result;
}

DatabaseManager::GroupMember InMemoryStorage::getGroupMember(const std::string& group_id, const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::string key = group_id + "_" + user_id;
    auto it = group_members_.find(key);
    
    if (it != group_members_.end()) {
        return it->second;
    }
    
    return DatabaseManager::GroupMember{};
}

bool InMemoryStorage::sendGroupMessage(const std::string& group_id, const std::string& sender_id, 
                                      const std::string& content, const std::string& message_type,
                                      const std::string& file_path, const std::string& file_name,
                                      long long file_size, const std::string& reply_to_message_id,
                                      const std::string& forwarded_from_user_id,
                                      const std::string& forwarded_from_username,
                                      const std::string& forwarded_from_message_id,
                                      std::string* message_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (groups_.find(group_id) == groups_.end()) {
        return false;
    }
    
    std::string msg_id = generateUUID();
    if (message_id) {
        *message_id = msg_id;
    }
    
    User sender = getUserById(sender_id);
    if (sender.id.empty()) {
        return false;
    }
    
    DatabaseManager::GroupMessage msg;
    msg.id = msg_id;
    msg.group_id = group_id;
    msg.sender_id = sender_id;
    msg.sender_username = sender.username;
    msg.content = content;
    msg.message_type = message_type;
    msg.file_path = file_path;
    msg.file_name = file_name;
    msg.file_size = file_size;
    msg.reply_to_message_id = reply_to_message_id;
    msg.forwarded_from_user_id = forwarded_from_user_id;
    msg.forwarded_from_username = forwarded_from_username;
    msg.forwarded_from_message_id = forwarded_from_message_id;
    msg.is_pinned = false;
    msg.created_at = getCurrentTimestamp();
    
    group_messages_[msg_id] = msg;
    group_message_list_[group_id].push_back(msg_id);
    
    return true;
}

std::vector<DatabaseManager::GroupMessage> InMemoryStorage::getGroupMessages(const std::string& group_id, int limit) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<DatabaseManager::GroupMessage> result;
    auto it = group_message_list_.find(group_id);
    
    if (it != group_message_list_.end()) {
        const auto& message_ids = it->second;
        int count = std::min(limit, static_cast<int>(message_ids.size()));
        
        // Get last N messages
        for (int i = message_ids.size() - count; i < static_cast<int>(message_ids.size()); i++) {
            auto msg_it = group_messages_.find(message_ids[i]);
            if (msg_it != group_messages_.end()) {
                result.push_back(msg_it->second);
            }
        }
    }
    
    return result;
}

bool InMemoryStorage::pinGroupMessage(const std::string& group_id, const std::string& message_id, const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = group_messages_.find(message_id);
    if (it != group_messages_.end() && it->second.group_id == group_id) {
        it->second.is_pinned = true;
        pinned_messages_[group_id + ":" + message_id] = user_id;
        return true;
    }
    
    return false;
}

bool InMemoryStorage::unpinGroupMessage(const std::string& group_id, const std::string& message_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = group_messages_.find(message_id);
    if (it != group_messages_.end() && it->second.group_id == group_id) {
        it->second.is_pinned = false;
        pinned_messages_.erase(group_id + ":" + message_id);
        return true;
    }
    
    return false;
}

std::string InMemoryStorage::createGroupInviteLink(const std::string& group_id, const std::string& creator_id, int expires_in_seconds) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (groups_.find(group_id) == groups_.end()) {
        return "";
    }
    
    // Generate random invite link
    const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.length() - 1);
    
    std::string invite_link;
    for (int i = 0; i < 32; i++) {
        invite_link += chars[dis(gen)];
    }
    
    groups_[group_id].invite_link = invite_link;
    group_invite_links_[invite_link] = group_id;
    
    if (expires_in_seconds > 0) {
        int64_t expire_time = getCurrentTimestampInt() + expires_in_seconds;
        group_invite_expires_[invite_link] = expire_time;
    }
    
    return invite_link;
}

std::string InMemoryStorage::joinGroupByInviteLink(const std::string& invite_link, const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = group_invite_links_.find(invite_link);
    if (it == group_invite_links_.end()) {
        return "";
    }
    
    // Check expiration
    auto expire_it = group_invite_expires_.find(invite_link);
    if (expire_it != group_invite_expires_.end()) {
        if (getCurrentTimestampInt() > expire_it->second) {
            return ""; // Expired
        }
    }
    
    std::string group_id = it->second;
    if (addGroupMember(group_id, user_id, "member")) {
        return group_id;
    }
    return "";
}

bool InMemoryStorage::updateGroupName(const std::string& group_id, const std::string& new_name) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = groups_.find(group_id);
    if (it != groups_.end()) {
        it->second.name = new_name;
        return true;
    }
    
    return false;
}

bool InMemoryStorage::updateGroupDescription(const std::string& group_id, const std::string& new_description) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = groups_.find(group_id);
    if (it != groups_.end()) {
        it->second.description = new_description;
        return true;
    }
    
    return false;
}

// ========== CHANNEL OPERATIONS ==========

bool InMemoryStorage::createChannel(const std::string& name, const std::string& description,
                                    const std::string& creator_id, std::string& channel_id,
                                    const std::string& custom_link) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Check if custom_link already exists
    if (!custom_link.empty()) {
        if (checkChannelCustomLinkExists(custom_link)) {
            return false;
        }
    }
    
    channel_id = generateUUID();
    
    DatabaseManager::Channel channel;
    channel.id = channel_id;
    channel.name = name;
    channel.description = description;
    channel.creator_id = creator_id;
    channel.custom_link = custom_link;
    channel.is_private = false;
    channel.show_author = true;
    channel.created_at = getCurrentTimestamp();
    
    channels_[channel_id] = channel;
    user_channels_[creator_id].push_back(channel_id);
    
    if (!custom_link.empty()) {
        channel_custom_links_[custom_link] = channel_id;
    }
    
    // Add creator as member with role "creator"
    DatabaseManager::ChannelMember member;
    member.id = generateUUID();
    member.channel_id = channel_id;
    member.user_id = creator_id;
    member.username = getUserById(creator_id).username;
    member.role = "creator";
    member.is_banned = false;
    member.joined_at = getCurrentTimestamp();
    
    channel_members_[channel_id + "_" + creator_id] = member;
    channel_member_list_[channel_id].push_back(creator_id);
    
    return true;
}

bool InMemoryStorage::checkChannelCustomLinkExists(const std::string& custom_link) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (custom_link.empty()) {
        return false;
    }
    
    return channel_custom_links_.find(custom_link) != channel_custom_links_.end();
}

DatabaseManager::Channel InMemoryStorage::getChannelById(const std::string& channel_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = channels_.find(channel_id);
    if (it != channels_.end()) {
        return it->second;
    }
    
    return DatabaseManager::Channel{};
}

DatabaseManager::Channel InMemoryStorage::getChannelByCustomLink(const std::string& custom_link) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = channel_custom_links_.find(custom_link);
    if (it != channel_custom_links_.end()) {
        return getChannelById(it->second);
    }
    
    return DatabaseManager::Channel{};
}

std::vector<DatabaseManager::Channel> InMemoryStorage::getUserChannels(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<DatabaseManager::Channel> result;
    auto it = user_channels_.find(user_id);
    
    if (it != user_channels_.end()) {
        for (const auto& channel_id : it->second) {
            auto channel_it = channels_.find(channel_id);
            if (channel_it != channels_.end()) {
                result.push_back(channel_it->second);
            }
        }
    }
    
    return result;
}

bool InMemoryStorage::addChannelMember(const std::string& channel_id, const std::string& user_id, const std::string& role) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (channels_.find(channel_id) == channels_.end()) {
        return false;
    }
    
    std::string key = channel_id + "_" + user_id;
    if (channel_members_.find(key) != channel_members_.end()) {
        return false; // Already a member
    }
    
    User user = getUserById(user_id);
    if (user.id.empty()) {
        return false;
    }
    
    DatabaseManager::ChannelMember member;
    member.id = generateUUID();
    member.channel_id = channel_id;
    member.user_id = user_id;
    member.username = user.username;
    member.role = role;
    member.is_banned = false;
    member.joined_at = getCurrentTimestamp();
    
    channel_members_[key] = member;
    channel_member_list_[channel_id].push_back(user_id);
    user_channels_[user_id].push_back(channel_id);
    
    return true;
}

bool InMemoryStorage::removeChannelMember(const std::string& channel_id, const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::string key = channel_id + "_" + user_id;
    auto it = channel_members_.find(key);
    
    if (it == channel_members_.end()) {
        return false;
    }
    
    channel_members_.erase(it);
    
    // Remove from channel member list
    auto& member_list = channel_member_list_[channel_id];
    member_list.erase(std::remove(member_list.begin(), member_list.end(), user_id), member_list.end());
    
    // Remove from user channels
    auto& user_channels_list = user_channels_[user_id];
    user_channels_list.erase(std::remove(user_channels_list.begin(), user_channels_list.end(), channel_id), user_channels_list.end());
    
    return true;
}

bool InMemoryStorage::updateChannelMemberRole(const std::string& channel_id, const std::string& user_id, const std::string& role) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::string key = channel_id + "_" + user_id;
    auto it = channel_members_.find(key);
    
    if (it != channel_members_.end()) {
        it->second.role = role;
        return true;
    }
    
    return false;
}

bool InMemoryStorage::banChannelMember(const std::string& channel_id, const std::string& user_id, bool banned) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::string key = channel_id + "_" + user_id;
    auto it = channel_members_.find(key);
    
    if (it != channel_members_.end()) {
        it->second.is_banned = banned;
        return true;
    }
    
    return false;
}

std::vector<DatabaseManager::ChannelMember> InMemoryStorage::getChannelMembers(const std::string& channel_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<DatabaseManager::ChannelMember> result;
    auto it = channel_member_list_.find(channel_id);
    
    if (it != channel_member_list_.end()) {
        for (const auto& user_id : it->second) {
            std::string key = channel_id + "_" + user_id;
            auto member_it = channel_members_.find(key);
            if (member_it != channel_members_.end()) {
                result.push_back(member_it->second);
            }
        }
    }
    
    return result;
}

DatabaseManager::ChannelMember InMemoryStorage::getChannelMember(const std::string& channel_id, const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::string key = channel_id + "_" + user_id;
    auto it = channel_members_.find(key);
    
    if (it != channel_members_.end()) {
        return it->second;
    }
    
    return DatabaseManager::ChannelMember{};
}

int InMemoryStorage::countChannelSubscribers(const std::string& channel_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    int count = 0;
    auto it = channel_member_list_.find(channel_id);
    
    if (it != channel_member_list_.end()) {
        for (const auto& user_id : it->second) {
            std::string key = channel_id + "_" + user_id;
            auto member_it = channel_members_.find(key);
            if (member_it != channel_members_.end() && member_it->second.role == "subscriber" && !member_it->second.is_banned) {
                count++;
            }
        }
    }
    
    return count;
}

int InMemoryStorage::countChannelMembers(const std::string& channel_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = channel_member_list_.find(channel_id);
    
    if (it != channel_member_list_.end()) {
        return static_cast<int>(it->second.size());
    }
    
    return 0;
}

bool InMemoryStorage::sendChannelMessage(const std::string& channel_id, const std::string& sender_id,
                                        const std::string& content, const std::string& message_type,
                                        const std::string& file_path, const std::string& file_name,
                                        long long file_size, std::string* message_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (channels_.find(channel_id) == channels_.end()) {
        return false;
    }
    
    std::string msg_id = generateUUID();
    if (message_id) {
        *message_id = msg_id;
    }
    
    User sender = getUserById(sender_id);
    if (sender.id.empty()) {
        return false;
    }
    
    DatabaseManager::ChannelMessage msg;
    msg.id = msg_id;
    msg.channel_id = channel_id;
    msg.sender_id = sender_id;
    msg.sender_username = sender.username;
    msg.content = content;
    msg.message_type = message_type;
    msg.file_path = file_path;
    msg.file_name = file_name;
    msg.file_size = file_size;
    msg.is_pinned = false;
    msg.views_count = 0;
    msg.created_at = getCurrentTimestamp();
    
    channel_messages_[msg_id] = msg;
    channel_message_list_[channel_id].push_back(msg_id);
    
    return true;
}

std::vector<DatabaseManager::ChannelMessage> InMemoryStorage::getChannelMessages(const std::string& channel_id, int limit) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<DatabaseManager::ChannelMessage> result;
    auto it = channel_message_list_.find(channel_id);
    
    if (it != channel_message_list_.end()) {
        const auto& message_ids = it->second;
        int count = std::min(limit, static_cast<int>(message_ids.size()));
        
        // Get last N messages
        for (int i = message_ids.size() - count; i < static_cast<int>(message_ids.size()); i++) {
            auto msg_it = channel_messages_.find(message_ids[i]);
            if (msg_it != channel_messages_.end()) {
                result.push_back(msg_it->second);
            }
        }
    }
    
    return result;
}

bool InMemoryStorage::pinChannelMessage(const std::string& channel_id, const std::string& message_id, const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = channel_messages_.find(message_id);
    if (it != channel_messages_.end() && it->second.channel_id == channel_id) {
        it->second.is_pinned = true;
        pinned_channel_messages_[channel_id + ":" + message_id] = user_id;
        return true;
    }
    
    return false;
}

bool InMemoryStorage::unpinChannelMessage(const std::string& channel_id, const std::string& message_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = channel_messages_.find(message_id);
    if (it != channel_messages_.end() && it->second.channel_id == channel_id) {
        it->second.is_pinned = false;
        pinned_channel_messages_.erase(channel_id + ":" + message_id);
        return true;
    }
    
    return false;
}

bool InMemoryStorage::setChannelCustomLink(const std::string& channel_id, const std::string& custom_link) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = channels_.find(channel_id);
    if (it == channels_.end()) {
        return false;
    }
    
    // Remove old custom link
    if (!it->second.custom_link.empty()) {
        channel_custom_links_.erase(it->second.custom_link);
    }
    
    // Set new custom link
    if (!custom_link.empty()) {
        if (checkChannelCustomLinkExists(custom_link)) {
            return false; // Already exists
        }
        channel_custom_links_[custom_link] = channel_id;
    }
    
    it->second.custom_link = custom_link;
    return true;
}

bool InMemoryStorage::updateChannelName(const std::string& channel_id, const std::string& new_name) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = channels_.find(channel_id);
    if (it != channels_.end()) {
        it->second.name = new_name;
        return true;
    }
    
    return false;
}

bool InMemoryStorage::updateChannelDescription(const std::string& channel_id, const std::string& new_description) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = channels_.find(channel_id);
    if (it != channels_.end()) {
        it->second.description = new_description;
        return true;
    }
    
    return false;
}

bool InMemoryStorage::setChannelPrivacy(const std::string& channel_id, bool is_private) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = channels_.find(channel_id);
    if (it != channels_.end()) {
        it->second.is_private = is_private;
        return true;
    }
    
    return false;
}

bool InMemoryStorage::setChannelShowAuthor(const std::string& channel_id, bool show_author) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = channels_.find(channel_id);
    if (it != channels_.end()) {
        it->second.show_author = show_author;
        return true;
    }
    
    return false;
}

bool InMemoryStorage::addMessageReaction(const std::string& message_id, const std::string& user_id, const std::string& reaction) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Check if user already reacted with this reaction
    auto& reactions = message_reactions_[message_id];
    for (auto& pair : reactions) {
        if (pair.first == user_id && pair.second == reaction) {
            return false; // Already reacted
        }
    }
    
    reactions.push_back({user_id, reaction});
    return true;
}

bool InMemoryStorage::removeMessageReaction(const std::string& message_id, const std::string& user_id, const std::string& reaction) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto& reactions = message_reactions_[message_id];
    reactions.erase(
        std::remove_if(reactions.begin(), reactions.end(),
            [&](const std::pair<std::string, std::string>& p) {
                return p.first == user_id && p.second == reaction;
            }),
        reactions.end()
    );
    
    return true;
}

std::vector<DatabaseManager::MessageReaction> InMemoryStorage::getMessageReactions(const std::string& message_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<DatabaseManager::MessageReaction> result;
    auto it = message_reactions_.find(message_id);
    
    if (it != message_reactions_.end()) {
        // Count reactions by type
        std::map<std::string, int> reaction_counts;
        for (const auto& pair : it->second) {
            reaction_counts[pair.second]++;
        }
        
        // Convert to MessageReaction vector
        for (const auto& pair : reaction_counts) {
            DatabaseManager::MessageReaction mr;
            mr.reaction = pair.first;
            mr.count = pair.second;
            result.push_back(mr);
        }
    }
    
    return result;
}

bool InMemoryStorage::addMessageView(const std::string& message_id, const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto& views = message_views_[message_id];
    
    // Check if user already viewed
    if (std::find(views.begin(), views.end(), user_id) != views.end()) {
        return false; // Already viewed
    }
    
    views.push_back(user_id);
    
    // Update views count in channel message
    for (auto& pair : channel_messages_) {
        if (pair.second.id == message_id) {
            pair.second.views_count = static_cast<int>(views.size());
            break;
        }
    }
    
    return true;
}

int InMemoryStorage::getMessageViewsCount(const std::string& message_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = message_views_.find(message_id);
    if (it != message_views_.end()) {
        return static_cast<int>(it->second.size());
    }
    
    return 0;
}

bool InMemoryStorage::addAllowedReaction(const std::string& channel_id, const std::string& reaction) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto& reactions = channel_allowed_reactions_[channel_id];
    
    if (std::find(reactions.begin(), reactions.end(), reaction) != reactions.end()) {
        return false; // Already added
    }
    
    reactions.push_back(reaction);
    return true;
}

bool InMemoryStorage::removeAllowedReaction(const std::string& channel_id, const std::string& reaction) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto& reactions = channel_allowed_reactions_[channel_id];
    reactions.erase(std::remove(reactions.begin(), reactions.end(), reaction), reactions.end());
    
    return true;
}

std::vector<std::string> InMemoryStorage::getAllowedReactions(const std::string& channel_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = channel_allowed_reactions_.find(channel_id);
    if (it != channel_allowed_reactions_.end()) {
        return it->second;
    }
    
    return std::vector<std::string>{};
}

bool InMemoryStorage::createChannelJoinRequest(const std::string& channel_id, const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Check if already a member
    if (channel_members_.find(channel_id + "_" + user_id) != channel_members_.end()) {
        return false;
    }
    
    // Check if request already exists
    auto& requests = channel_join_requests_[channel_id];
    for (const auto& req : requests) {
        if (req.user_id == user_id) {
            return false; // Request already exists
        }
    }
    
    User user = getUserById(user_id);
    if (user.id.empty()) {
        return false;
    }
    
    DatabaseManager::ChannelMember request;
    request.id = generateUUID();
    request.channel_id = channel_id;
    request.user_id = user_id;
    request.username = user.username;
    request.role = "pending";
    request.is_banned = false;
    request.joined_at = getCurrentTimestamp();
    
    requests.push_back(request);
    return true;
}

bool InMemoryStorage::acceptChannelJoinRequest(const std::string& channel_id, const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto& requests = channel_join_requests_[channel_id];
    
    // Find and remove request
    auto it = std::remove_if(requests.begin(), requests.end(),
        [&](const DatabaseManager::ChannelMember& req) {
            return req.user_id == user_id;
        });
    
    if (it == requests.end()) {
        return false; // Request not found
    }
    
    requests.erase(it, requests.end());
    
    // Add as subscriber
    return addChannelMember(channel_id, user_id, "subscriber");
}

bool InMemoryStorage::rejectChannelJoinRequest(const std::string& channel_id, const std::string& user_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto& requests = channel_join_requests_[channel_id];
    
    auto it = std::remove_if(requests.begin(), requests.end(),
        [&](const DatabaseManager::ChannelMember& req) {
            return req.user_id == user_id;
        });
    
    if (it == requests.end()) {
        return false;
    }
    
    requests.erase(it, requests.end());
    return true;
}

std::vector<DatabaseManager::ChannelMember> InMemoryStorage::getChannelJoinRequests(const std::string& channel_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = channel_join_requests_.find(channel_id);
    if (it != channel_join_requests_.end()) {
        return it->second;
    }
    
    return std::vector<DatabaseManager::ChannelMember>{};
}

bool InMemoryStorage::createChannelChat(const std::string& channel_id, std::string& group_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto channel_it = channels_.find(channel_id);
    if (channel_it == channels_.end()) {
        return false;
    }
    
    // Create group for channel chat
    std::string chat_name = "Chat: " + channel_it->second.name;
    std::string chat_description = "Chat for channel " + channel_it->second.name;
    
    if (!createGroup(chat_name, chat_description, channel_it->second.creator_id, group_id)) {
        return false;
    }
    
    // Store mapping (would need additional storage structure)
    // For now, just return success
    return true;
}

// ========== BOT API OPERATIONS ==========

bool InMemoryStorage::createBot(const std::string& owner_id, const std::string& username, 
                                const std::string& first_name, std::string& bot_id, std::string& token) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Validate username ends with 'bot'
    if (username.length() < 4 || username.substr(username.length() - 3) != "bot") {
        return false;
    }
    
    // Check if username already exists
    if (bot_username_to_id_.find(username) != bot_username_to_id_.end()) {
        return false;
    }
    
    bot_id = generateUUID();
    token = generateToken();
    
    Bot bot;
    bot.id = bot_id;
    bot.owner_id = owner_id;
    bot.username = username;
    bot.token = token;
    bot.first_name = first_name;
    bot.description = "";
    bot.is_active = true;
    bot.created_at = getCurrentTimestamp();
    bot.webhook_url = "";
    bot.webhook_secret_token = "";
    bot.allowed_updates = std::vector<std::string>{};
    
    bots_[bot_id] = bot;
    bot_token_to_id_[token] = bot_id;
    bot_username_to_id_[username] = bot_id;
    bot_last_update_id_[token] = 0;
    
    return true;
}

InMemoryStorage::Bot InMemoryStorage::getBotByToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = bot_token_to_id_.find(token);
    if (it != bot_token_to_id_.end()) {
        auto bot_it = bots_.find(it->second);
        if (bot_it != bots_.end()) {
            return bot_it->second;
        }
    }
    
    return Bot{};
}

InMemoryStorage::Bot InMemoryStorage::getBotById(const std::string& bot_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = bots_.find(bot_id);
    if (it != bots_.end()) {
        return it->second;
    }
    
    return Bot{};
}

InMemoryStorage::Bot InMemoryStorage::getBotByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = bot_username_to_id_.find(username);
    if (it != bot_username_to_id_.end()) {
        return getBotById(it->second);
    }
    
    return Bot{};
}

bool InMemoryStorage::deleteBot(const std::string& bot_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = bots_.find(bot_id);
    if (it == bots_.end()) {
        return false;
    }
    
    // Remove from all mappings
    bot_token_to_id_.erase(it->second.token);
    bot_username_to_id_.erase(it->second.username);
    bot_updates_.erase(it->second.token);
    bot_last_update_id_.erase(it->second.token);
    bot_webhooks_.erase(it->second.token);
    
    bots_.erase(it);
    return true;
}

bool InMemoryStorage::updateBotInfo(const std::string& bot_id, const std::string& first_name, const std::string& description) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = bots_.find(bot_id);
    if (it == bots_.end()) {
        return false;
    }
    
    if (!first_name.empty()) {
        it->second.first_name = first_name;
    }
    if (!description.empty()) {
        it->second.description = description;
    }
    
    return true;
}

bool InMemoryStorage::addUpdate(const std::string& bot_token, const std::string& update_type,
                                const std::string& update_data, int64_t& update_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    // Check if bot exists
    if (bot_token_to_id_.find(bot_token) == bot_token_to_id_.end()) {
        return false;
    }
    
    update_id = update_id_counter_++;
    
    Update update;
    update.update_id = update_id;
    update.bot_token = bot_token;
    update.update_type = update_type;
    update.update_data = update_data;
    update.created_at = getCurrentTimestampInt();
    
    bot_updates_[bot_token].push_back(update);
    bot_last_update_id_[bot_token] = update_id;
    
    return true;
}

std::vector<InMemoryStorage::Update> InMemoryStorage::getUpdates(const std::string& bot_token, int64_t offset, 
                                                                 int limit, int timeout) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    std::vector<Update> result;
    auto it = bot_updates_.find(bot_token);
    
    if (it != bot_updates_.end()) {
        for (const auto& update : it->second) {
            if (update.update_id >= offset) {
                result.push_back(update);
                if (result.size() >= static_cast<size_t>(limit)) {
                    break;
                }
            }
        }
    }
    
    return result;
}

bool InMemoryStorage::confirmUpdate(const std::string& bot_token, int64_t update_id) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = bot_updates_.find(bot_token);
    if (it == bot_updates_.end()) {
        return false;
    }
    
    // Remove confirmed updates (up to update_id)
    auto& updates = it->second;
    updates.erase(
        std::remove_if(updates.begin(), updates.end(),
            [&](const Update& u) {
                return u.update_id <= update_id;
            }),
        updates.end()
    );
    
    return true;
}

bool InMemoryStorage::setWebhook(const std::string& bot_token, const std::string& url,
                                 const std::string& secret_token, int max_connections,
                                 const std::vector<std::string>& allowed_updates) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto bot_id_it = bot_token_to_id_.find(bot_token);
    if (bot_id_it == bot_token_to_id_.end()) {
        return false;
    }
    
    auto bot_it = bots_.find(bot_id_it->second);
    if (bot_it == bots_.end()) {
        return false;
    }
    
    bot_it->second.webhook_url = url;
    bot_it->second.webhook_secret_token = secret_token;
    bot_it->second.allowed_updates = allowed_updates;
    
    WebhookInfo info;
    info.url = url;
    info.has_custom_certificate = false;
    info.pending_update_count = static_cast<int>(bot_updates_[bot_token].size());
    info.last_error_date = "";
    info.last_error_message = "";
    info.max_connections = max_connections;
    info.allowed_updates = allowed_updates;
    
    bot_webhooks_[bot_token] = info;
    
    return true;
}

bool InMemoryStorage::deleteWebhook(const std::string& bot_token, bool drop_pending_updates) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto bot_id_it = bot_token_to_id_.find(bot_token);
    if (bot_id_it == bot_token_to_id_.end()) {
        return false;
    }
    
    auto bot_it = bots_.find(bot_id_it->second);
    if (bot_it == bots_.end()) {
        return false;
    }
    
    bot_it->second.webhook_url = "";
    bot_it->second.webhook_secret_token = "";
    bot_it->second.allowed_updates.clear();
    
    if (drop_pending_updates) {
        bot_updates_[bot_token].clear();
    }
    
    WebhookInfo info;
    info.url = "";
    info.has_custom_certificate = false;
    info.pending_update_count = drop_pending_updates ? 0 : static_cast<int>(bot_updates_[bot_token].size());
    info.last_error_date = "";
    info.last_error_message = "";
    info.max_connections = 0;
    info.allowed_updates.clear();
    
    bot_webhooks_[bot_token] = info;
    
    return true;
}

InMemoryStorage::WebhookInfo InMemoryStorage::getWebhookInfo(const std::string& bot_token) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    auto it = bot_webhooks_.find(bot_token);
    if (it != bot_webhooks_.end()) {
        return it->second;
    }
    
    // Return empty webhook info if not set
    WebhookInfo info;
    info.url = "";
    info.has_custom_certificate = false;
    info.pending_update_count = static_cast<int>(bot_updates_[bot_token].size());
    info.last_error_date = "";
    info.last_error_message = "";
    info.max_connections = 0;
    info.allowed_updates.clear();
    
    return info;
}

} // namespace xipher
