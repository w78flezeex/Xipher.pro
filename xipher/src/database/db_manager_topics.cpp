#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>
#include <cstring>
#include <algorithm>

namespace xipher {

bool DatabaseManager::setGroupForumMode(const std::string& group_id, bool enabled) {
    const std::string enabled_str = enabled ? "true" : "false";
    const char* param_values[2] = {enabled_str.c_str(), group_id.c_str()};
    
    std::string query = "UPDATE groups SET forum_mode = $1 WHERE id = $2";
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 2, nullptr, param_values, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("setGroupForumMode failed: " + std::string(PQresultErrorMessage(res)));
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    
    // Create General topic if enabling forum mode
    if (enabled) {
        // Check if General topic exists
        const char* check_params[1] = {group_id.c_str()};
        std::string check_query = "SELECT id FROM group_topics WHERE group_id = $1 AND is_general = true";
        PGresult* check_res = PQexecParams(db_->getConnection(), check_query.c_str(), 1, nullptr, check_params, nullptr, nullptr, 0);
        
        if (check_res && PQresultStatus(check_res) == PGRES_TUPLES_OK && PQntuples(check_res) == 0) {
            // Create General topic
            std::string topic_id;
            createGroupTopic(group_id, "General", "💬", "#9333ea", "", true, topic_id);
        }
        if (check_res) PQclear(check_res);
    }
    
    Logger::getInstance().info("Group " + group_id + " forum mode set to " + enabled_str);
    return true;
}

bool DatabaseManager::isGroupForumMode(const std::string& group_id) {
    const char* param_values[1] = {group_id.c_str()};
    std::string query = "SELECT COALESCE(forum_mode, false) FROM groups WHERE id = $1";
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 1, nullptr, param_values, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    
    bool result = PQgetvalue(res, 0, 0)[0] == 't';
    PQclear(res);
    return result;
}

bool DatabaseManager::createGroupTopic(const std::string& group_id, const std::string& name, 
                                       const std::string& icon_emoji, const std::string& icon_color,
                                       const std::string& created_by, bool is_general, std::string& topic_id) {
    const std::string is_general_str = is_general ? "true" : "false";
    const char* param_values[6] = {
        group_id.c_str(), 
        name.c_str(), 
        icon_emoji.c_str(), 
        icon_color.c_str(),
        created_by.empty() ? nullptr : created_by.c_str(),
        is_general_str.c_str()
    };
    
    std::string query = R"(
        INSERT INTO group_topics (group_id, name, icon_emoji, icon_color, created_by, is_general)
        VALUES ($1, $2, $3, $4, $5, $6)
        RETURNING id
    )";
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 6, nullptr, param_values, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        Logger::getInstance().error("createGroupTopic failed: " + std::string(res ? PQresultErrorMessage(res) : "null result"));
        if (res) PQclear(res);
        return false;
    }
    
    topic_id = std::string(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    Logger::getInstance().info("Topic created: " + topic_id + " in group " + group_id);
    return true;
}

bool DatabaseManager::updateGroupTopic(const std::string& topic_id, const std::string& name,
                                       const std::string& icon_emoji, const std::string& icon_color, bool is_closed) {
    const std::string is_closed_str = is_closed ? "true" : "false";
    const char* param_values[5] = {name.c_str(), icon_emoji.c_str(), icon_color.c_str(), is_closed_str.c_str(), topic_id.c_str()};
    
    std::string query = "UPDATE group_topics SET name = $1, icon_emoji = $2, icon_color = $3, is_closed = $4 WHERE id = $5";
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 5, nullptr, param_values, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("updateGroupTopic failed: " + std::string(PQresultErrorMessage(res)));
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool DatabaseManager::deleteGroupTopic(const std::string& topic_id) {
    // Don't allow deleting General topic
    const char* check_params[1] = {topic_id.c_str()};
    std::string check_query = "SELECT is_general FROM group_topics WHERE id = $1";
    PGresult* check_res = PQexecParams(db_->getConnection(), check_query.c_str(), 1, nullptr, check_params, nullptr, nullptr, 0);
    
    if (check_res && PQresultStatus(check_res) == PGRES_TUPLES_OK && PQntuples(check_res) > 0) {
        if (PQgetvalue(check_res, 0, 0)[0] == 't') {
            Logger::getInstance().warning("Cannot delete General topic: " + topic_id);
            PQclear(check_res);
            return false;
        }
    }
    if (check_res) PQclear(check_res);
    
    const char* param_values[1] = {topic_id.c_str()};
    std::string query = "DELETE FROM group_topics WHERE id = $1";
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 1, nullptr, param_values, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("deleteGroupTopic failed: " + std::string(PQresultErrorMessage(res)));
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool DatabaseManager::pinGroupTopic(const std::string& topic_id, int order) {
    const std::string order_str = std::to_string(order);
    const char* param_values[2] = {order_str.c_str(), topic_id.c_str()};
    
    std::string query = "UPDATE group_topics SET pinned_order = $1 WHERE id = $2";
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 2, nullptr, param_values, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool DatabaseManager::unpinGroupTopic(const std::string& topic_id) {
    return pinGroupTopic(topic_id, 0);
}

std::vector<DatabaseManager::GroupTopic> DatabaseManager::getGroupTopics(const std::string& group_id) {
    std::vector<GroupTopic> topics;
    const char* param_values[1] = {group_id.c_str()};
    
    std::string query = R"(
        SELECT id, group_id, name, COALESCE(icon_emoji, ''), COALESCE(icon_color, '#9333ea'), 
               COALESCE(is_general, false), COALESCE(is_closed, false), COALESCE(pinned_order, 0),
               COALESCE(last_message_at, created_at)::text, COALESCE(message_count, 0), 
               COALESCE(created_by::text, ''), created_at::text,
               COALESCE(is_hidden, false), COALESCE(last_message_content, ''), 
               COALESCE(last_message_sender, ''), COALESCE(last_message_sender_id::text, ''),
               COALESCE(unread_count, 0)
        FROM group_topics 
        WHERE group_id = $1
        ORDER BY is_general DESC, pinned_order DESC, last_message_at DESC
    )";
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 1, nullptr, param_values, nullptr, nullptr, 0);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            GroupTopic topic;
            topic.id = std::string(PQgetvalue(res, i, 0));
            topic.group_id = std::string(PQgetvalue(res, i, 1));
            topic.name = std::string(PQgetvalue(res, i, 2));
            topic.icon_emoji = std::string(PQgetvalue(res, i, 3));
            topic.icon_color = std::string(PQgetvalue(res, i, 4));
            topic.is_general = PQgetvalue(res, i, 5)[0] == 't';
            topic.is_closed = PQgetvalue(res, i, 6)[0] == 't';
            topic.pinned_order = std::stoi(std::string(PQgetvalue(res, i, 7)));
            topic.last_message_at = std::string(PQgetvalue(res, i, 8));
            topic.message_count = std::stoi(std::string(PQgetvalue(res, i, 9)));
            topic.created_by = std::string(PQgetvalue(res, i, 10));
            topic.created_at = std::string(PQgetvalue(res, i, 11));
            topic.is_hidden = PQgetvalue(res, i, 12)[0] == 't';
            topic.last_message_content = std::string(PQgetvalue(res, i, 13));
            topic.last_message_sender = std::string(PQgetvalue(res, i, 14));
            topic.last_message_sender_id = std::string(PQgetvalue(res, i, 15));
            topic.unread_count = std::stoi(std::string(PQgetvalue(res, i, 16)));
            topics.push_back(topic);
        }
    }
    
    if (res) PQclear(res);
    return topics;
}

DatabaseManager::GroupTopic DatabaseManager::getGroupTopicById(const std::string& topic_id) {
    GroupTopic topic;
    const char* param_values[1] = {topic_id.c_str()};
    
    std::string query = R"(
        SELECT id, group_id, name, COALESCE(icon_emoji, ''), COALESCE(icon_color, '#9333ea'), 
               COALESCE(is_general, false), COALESCE(is_closed, false), COALESCE(pinned_order, 0),
               COALESCE(last_message_at, created_at)::text, COALESCE(message_count, 0), 
               COALESCE(created_by::text, ''), created_at::text,
               COALESCE(is_hidden, false), COALESCE(last_message_content, ''), 
               COALESCE(last_message_sender, ''), COALESCE(last_message_sender_id::text, ''),
               COALESCE(unread_count, 0)
        FROM group_topics 
        WHERE id = $1
    )";
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 1, nullptr, param_values, nullptr, nullptr, 0);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        topic.id = std::string(PQgetvalue(res, 0, 0));
        topic.group_id = std::string(PQgetvalue(res, 0, 1));
        topic.name = std::string(PQgetvalue(res, 0, 2));
        topic.icon_emoji = std::string(PQgetvalue(res, 0, 3));
        topic.icon_color = std::string(PQgetvalue(res, 0, 4));
        topic.is_general = PQgetvalue(res, 0, 5)[0] == 't';
        topic.is_closed = PQgetvalue(res, 0, 6)[0] == 't';
        topic.pinned_order = std::stoi(std::string(PQgetvalue(res, 0, 7)));
        topic.last_message_at = std::string(PQgetvalue(res, 0, 8));
        topic.message_count = std::stoi(std::string(PQgetvalue(res, 0, 9)));
        topic.created_by = std::string(PQgetvalue(res, 0, 10));
        topic.created_at = std::string(PQgetvalue(res, 0, 11));
        topic.is_hidden = PQgetvalue(res, 0, 12)[0] == 't';
        topic.last_message_content = std::string(PQgetvalue(res, 0, 13));
        topic.last_message_sender = std::string(PQgetvalue(res, 0, 14));
        topic.last_message_sender_id = std::string(PQgetvalue(res, 0, 15));
        topic.unread_count = std::stoi(std::string(PQgetvalue(res, 0, 16)));
    }
    
    if (res) PQclear(res);
    return topic;
}

bool DatabaseManager::sendGroupMessageToTopic(const std::string& group_id, const std::string& topic_id, 
                                              const std::string& sender_id, const std::string& content,
                                              const std::string& message_type, const std::string& file_path,
                                              const std::string& file_name, long long file_size,
                                              const std::string& reply_to_message_id) {
    const std::string file_size_str = std::to_string(file_size);
    const char* param_values[9] = {
        group_id.c_str(),
        topic_id.c_str(),
        sender_id.c_str(),
        content.c_str(),
        message_type.c_str(),
        file_path.empty() ? nullptr : file_path.c_str(),
        file_name.empty() ? nullptr : file_name.c_str(),
        file_size_str.c_str(),
        reply_to_message_id.empty() ? nullptr : reply_to_message_id.c_str()
    };
    
    std::string query = R"(
        INSERT INTO group_messages (group_id, topic_id, sender_id, content, message_type, file_path, file_name, file_size, reply_to_message_id)
        VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)
    )";
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 9, nullptr, param_values, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("sendGroupMessageToTopic failed: " + std::string(res ? PQresultErrorMessage(res) : "null result"));
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    
    // Update topic stats
    updateTopicMessageCount(topic_id);
    
    return true;
}

std::vector<DatabaseManager::GroupMessage> DatabaseManager::getTopicMessages(const std::string& topic_id, int limit) {
    std::vector<GroupMessage> messages;
    const std::string limit_str = std::to_string(limit);
    const char* param_values[2] = {topic_id.c_str(), limit_str.c_str()};
    
    std::string query = R"(
        SELECT m.id, m.group_id, m.sender_id, COALESCE(u.username, 'Unknown') as sender_username,
               m.content, COALESCE(m.message_type, 'text'), COALESCE(m.file_path, ''), 
               COALESCE(m.file_name, ''), COALESCE(m.file_size, 0), 
               COALESCE(m.reply_to_message_id::text, ''), 
               COALESCE(m.forwarded_from_user_id::text, ''), COALESCE(m.forwarded_from_username, ''),
               COALESCE(m.forwarded_from_message_id::text, ''),
               COALESCE(m.is_pinned, false), m.created_at::text
        FROM group_messages m
        LEFT JOIN users u ON m.sender_id = u.id
        WHERE m.topic_id = $1
        ORDER BY m.created_at DESC
        LIMIT $2
    )";
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 2, nullptr, param_values, nullptr, nullptr, 0);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            GroupMessage msg;
            msg.id = std::string(PQgetvalue(res, i, 0));
            msg.group_id = std::string(PQgetvalue(res, i, 1));
            msg.sender_id = std::string(PQgetvalue(res, i, 2));
            msg.sender_username = std::string(PQgetvalue(res, i, 3));
            msg.content = std::string(PQgetvalue(res, i, 4));
            msg.message_type = std::string(PQgetvalue(res, i, 5));
            msg.file_path = std::string(PQgetvalue(res, i, 6));
            msg.file_name = std::string(PQgetvalue(res, i, 7));
            msg.file_size = std::stoll(std::string(PQgetvalue(res, i, 8)));
            msg.reply_to_message_id = std::string(PQgetvalue(res, i, 9));
            msg.forwarded_from_user_id = std::string(PQgetvalue(res, i, 10));
            msg.forwarded_from_username = std::string(PQgetvalue(res, i, 11));
            msg.forwarded_from_message_id = std::string(PQgetvalue(res, i, 12));
            msg.is_pinned = PQgetvalue(res, i, 13)[0] == 't';
            msg.created_at = std::string(PQgetvalue(res, i, 14));
            messages.push_back(msg);
        }
    }
    
    if (res) PQclear(res);
    
    // Reverse to get chronological order
    std::reverse(messages.begin(), messages.end());
    return messages;
}

bool DatabaseManager::updateTopicMessageCount(const std::string& topic_id) {
    const char* param_values[1] = {topic_id.c_str()};
    
    std::string query = R"(
        UPDATE group_topics gt
        SET message_count = (SELECT COUNT(*) FROM group_messages WHERE topic_id = $1),
            last_message_at = CURRENT_TIMESTAMP,
            last_message_content = COALESCE((
                SELECT LEFT(m.content, 100) FROM group_messages m 
                WHERE m.topic_id = $1 
                ORDER BY m.created_at DESC LIMIT 1
            ), ''),
            last_message_sender = COALESCE((
                SELECT u.username FROM group_messages m 
                JOIN users u ON m.sender_id = u.id
                WHERE m.topic_id = $1 
                ORDER BY m.created_at DESC LIMIT 1
            ), ''),
            last_message_sender_id = (
                SELECT m.sender_id FROM group_messages m 
                WHERE m.topic_id = $1 
                ORDER BY m.created_at DESC LIMIT 1
            )
        WHERE gt.id = $1
    )";
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 1, nullptr, param_values, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool DatabaseManager::setTopicHidden(const std::string& topic_id, bool is_hidden) {
    std::string hidden_str = is_hidden ? "true" : "false";
    const char* param_values[2] = {hidden_str.c_str(), topic_id.c_str()};
    
    std::string query = R"(
        UPDATE group_topics 
        SET is_hidden = $1 
        WHERE id = $2 AND is_general = true
    )";
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 2, nullptr, param_values, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool DatabaseManager::setTopicClosed(const std::string& topic_id, bool is_closed) {
    std::string closed_str = is_closed ? "true" : "false";
    const char* param_values[2] = {closed_str.c_str(), topic_id.c_str()};
    
    std::string query = R"(
        UPDATE group_topics 
        SET is_closed = $1 
        WHERE id = $2
    )";
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 2, nullptr, param_values, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool DatabaseManager::reorderPinnedTopics(const std::string& group_id, const std::vector<std::string>& topic_ids) {
    // Update pinned_order for each topic based on position in the array
    // Higher order = more visible (at top)
    int order = topic_ids.size();
    
    for (const auto& topic_id : topic_ids) {
        std::string order_str = std::to_string(order);
        const char* param_values[3] = {order_str.c_str(), topic_id.c_str(), group_id.c_str()};
        
        std::string query = R"(
            UPDATE group_topics 
            SET pinned_order = $1 
            WHERE id = $2 AND group_id = $3
        )";
        
        PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 3, nullptr, param_values, nullptr, nullptr, 0);
        
        if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
            if (res) PQclear(res);
            return false;
        }
        
        PQclear(res);
        order--;
    }
    
    return true;
}

std::vector<DatabaseManager::GroupTopic> DatabaseManager::searchTopics(const std::string& group_id, const std::string& query_str) {
    std::vector<GroupTopic> topics;
    std::string search_pattern = "%" + query_str + "%";
    const char* param_values[2] = {group_id.c_str(), search_pattern.c_str()};
    
    std::string query = R"(
        SELECT id, group_id, name, COALESCE(icon_emoji, ''), COALESCE(icon_color, '#9333ea'), 
               COALESCE(is_general, false), COALESCE(is_closed, false), COALESCE(pinned_order, 0),
               COALESCE(last_message_at, created_at)::text, COALESCE(message_count, 0), 
               COALESCE(created_by::text, ''), created_at::text,
               COALESCE(is_hidden, false), COALESCE(last_message_content, ''), 
               COALESCE(last_message_sender, ''), COALESCE(last_message_sender_id::text, ''),
               COALESCE(unread_count, 0)
        FROM group_topics 
        WHERE group_id = $1 AND LOWER(name) LIKE LOWER($2)
        ORDER BY is_general DESC, pinned_order DESC, last_message_at DESC
    )";
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 2, nullptr, param_values, nullptr, nullptr, 0);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            GroupTopic topic;
            topic.id = std::string(PQgetvalue(res, i, 0));
            topic.group_id = std::string(PQgetvalue(res, i, 1));
            topic.name = std::string(PQgetvalue(res, i, 2));
            topic.icon_emoji = std::string(PQgetvalue(res, i, 3));
            topic.icon_color = std::string(PQgetvalue(res, i, 4));
            topic.is_general = PQgetvalue(res, i, 5)[0] == 't';
            topic.is_closed = PQgetvalue(res, i, 6)[0] == 't';
            topic.pinned_order = std::stoi(std::string(PQgetvalue(res, i, 7)));
            topic.last_message_at = std::string(PQgetvalue(res, i, 8));
            topic.message_count = std::stoi(std::string(PQgetvalue(res, i, 9)));
            topic.created_by = std::string(PQgetvalue(res, i, 10));
            topic.created_at = std::string(PQgetvalue(res, i, 11));
            topic.is_hidden = PQgetvalue(res, i, 12)[0] == 't';
            topic.last_message_content = std::string(PQgetvalue(res, i, 13));
            topic.last_message_sender = std::string(PQgetvalue(res, i, 14));
            topic.last_message_sender_id = std::string(PQgetvalue(res, i, 15));
            topic.unread_count = std::stoi(std::string(PQgetvalue(res, i, 16)));
            topics.push_back(topic);
        }
    }
    
    if (res) PQclear(res);
    return topics;
}

bool DatabaseManager::setTopicNotifications(const std::string& topic_id, const std::string& user_id, const std::string& mode) {
    // Store notification preferences in a separate table or JSON field
    // For now, we'll use a simple approach - store in topic_user_settings table
    const char* param_values[3] = {topic_id.c_str(), user_id.c_str(), mode.c_str()};
    
    // First, try to update existing record
    std::string query = R"(
        INSERT INTO topic_user_settings (topic_id, user_id, notification_mode, created_at)
        VALUES ($1, $2, $3, CURRENT_TIMESTAMP)
        ON CONFLICT (topic_id, user_id) 
        DO UPDATE SET notification_mode = $3
    )";
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 3, nullptr, param_values, nullptr, nullptr, 0);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        // Table might not exist, try to create it
        if (res) PQclear(res);
        
        std::string create_table = R"(
            CREATE TABLE IF NOT EXISTS topic_user_settings (
                topic_id UUID NOT NULL,
                user_id UUID NOT NULL,
                notification_mode VARCHAR(20) DEFAULT 'all',
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                PRIMARY KEY (topic_id, user_id)
            )
        )";
        
        PGresult* create_res = PQexec(db_->getConnection(), create_table.c_str());
        if (create_res) PQclear(create_res);
        
        // Retry the insert
        res = PQexecParams(db_->getConnection(), query.c_str(), 3, nullptr, param_values, nullptr, nullptr, 0);
        
        if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
            if (res) PQclear(res);
            return false;
        }
    }
    
    PQclear(res);
    return true;
}

DatabaseManager::UserChatStats DatabaseManager::getUserChatStats(const std::string& viewer_id, const std::string& target_id) {
    UserChatStats stats;
    
    // Count media by type in direct messages between two users
    std::string query = R"(
        SELECT 
            COUNT(*) FILTER (WHERE message_type = 'image') as photos,
            COUNT(*) FILTER (WHERE message_type = 'video') as videos,
            COUNT(*) FILTER (WHERE message_type = 'file') as files,
            COUNT(*) FILTER (WHERE message_type = 'audio') as audio,
            COUNT(*) FILTER (WHERE message_type = 'voice') as voice,
            COUNT(*) FILTER (WHERE message_type = 'text' AND content ~ 'https?://') as links,
            COUNT(*) FILTER (WHERE message_type = 'gif' OR (message_type = 'file' AND LOWER(file_name) LIKE '%.gif')) as gifs
        FROM messages 
        WHERE 
            ((sender_id = $1 AND receiver_id = $2) OR (sender_id = $2 AND receiver_id = $1))
            AND is_deleted = false
    )";
    
    const char* param_values[2] = {viewer_id.c_str(), target_id.c_str()};
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 2, nullptr, param_values, nullptr, nullptr, 0);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        stats.photo_count = atoll(PQgetvalue(res, 0, 0));
        stats.video_count = atoll(PQgetvalue(res, 0, 1));
        stats.file_count = atoll(PQgetvalue(res, 0, 2));
        stats.audio_count = atoll(PQgetvalue(res, 0, 3));
        stats.voice_count = atoll(PQgetvalue(res, 0, 4));
        stats.link_count = atoll(PQgetvalue(res, 0, 5));
        stats.gif_count = atoll(PQgetvalue(res, 0, 6));
    }
    if (res) PQclear(res);
    
    // Count saved messages (favorites) for the viewer
    if (viewer_id == target_id) {
        std::string saved_query = R"(
            SELECT COUNT(*) FROM messages 
            WHERE sender_id = $1 AND receiver_id = $1 AND is_deleted = false
        )";
        const char* saved_params[1] = {viewer_id.c_str()};
        PGresult* saved_res = PQexecParams(db_->getConnection(), saved_query.c_str(), 1, nullptr, saved_params, nullptr, nullptr, 0);
        if (saved_res && PQresultStatus(saved_res) == PGRES_TUPLES_OK && PQntuples(saved_res) > 0) {
            stats.saved_count = atoll(PQgetvalue(saved_res, 0, 0));
        }
        if (saved_res) PQclear(saved_res);
    }
    
    return stats;
}

std::vector<DatabaseManager::Group> DatabaseManager::getCommonGroups(const std::string& user1_id, const std::string& user2_id, int limit) {
    std::vector<Group> groups;
    
    std::string query = R"(
        SELECT DISTINCT g.id, g.name, g.description, g.invite_link, g.created_at, g.creator_id
        FROM groups g
        INNER JOIN group_members gm1 ON g.id = gm1.group_id AND gm1.user_id = $1 AND gm1.status = 'active'
        INNER JOIN group_members gm2 ON g.id = gm2.group_id AND gm2.user_id = $2 AND gm2.status = 'active'
        ORDER BY g.name
        LIMIT $3
    )";
    
    std::string limit_str = std::to_string(limit);
    const char* param_values[3] = {user1_id.c_str(), user2_id.c_str(), limit_str.c_str()};
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 3, nullptr, param_values, nullptr, nullptr, 0);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; i++) {
            Group g;
            g.id = PQgetvalue(res, i, 0);
            g.name = PQgetvalue(res, i, 1);
            g.description = PQgetvalue(res, i, 2);
            g.invite_link = PQgetvalue(res, i, 3);
            g.created_at = PQgetvalue(res, i, 4);
            g.creator_id = PQgetvalue(res, i, 5);
            groups.push_back(g);
        }
    }
    if (res) PQclear(res);
    
    return groups;
}

} // namespace xipher
