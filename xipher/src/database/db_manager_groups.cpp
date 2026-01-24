#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>
#include <cstring>
#include <random>
#include <ctime>

namespace xipher {

// Генерация случайной строки для invite link
std::string generateInviteLink() {
    const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.length() - 1);
    
    std::string result;
    for (int i = 0; i < 32; i++) {
        result += chars[dis(gen)];
    }
    return result;
}

bool DatabaseManager::createGroup(const std::string& name, const std::string& description, const std::string& creator_id, std::string& group_id) {
    const char* param_values[3] = {name.c_str(), description.c_str(), creator_id.c_str()};
    PGresult* res = db_->executePrepared("create_group", 3, param_values);
    
    if (!res) {
        Logger::getInstance().error("Failed to execute create_group prepared statement");
        return false;
    }
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        Logger::getInstance().error("create_group failed: " + std::string(PQresultErrorMessage(res)));
        PQclear(res);
        return false;
    }
    
    if (PQntuples(res) == 0) {
        Logger::getInstance().error("create_group returned no rows");
        PQclear(res);
        return false;
    }
    
    group_id = std::string(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    // Добавляем создателя как участника с ролью creator
    if (!addGroupMember(group_id, creator_id, "creator")) {
        Logger::getInstance().warning("Failed to add creator as group member, but group was created");
    }
    
    Logger::getInstance().info("Group created successfully: " + group_id);
    return true;
}

DatabaseManager::Group DatabaseManager::getGroupById(const std::string& group_id) {
    Group group;
    const char* param_values[1] = {group_id.c_str()};
    PGresult* res = db_->executePrepared("get_group_by_id", 1, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        group.id = std::string(PQgetvalue(res, 0, 0));
        group.name = std::string(PQgetvalue(res, 0, 1));
        group.description = PQgetisnull(res, 0, 2) ? "" : std::string(PQgetvalue(res, 0, 2));
        group.creator_id = std::string(PQgetvalue(res, 0, 3));
        group.invite_link = PQgetisnull(res, 0, 4) ? "" : std::string(PQgetvalue(res, 0, 4));
        group.created_at = std::string(PQgetvalue(res, 0, 5));
    }
    
    if (res) PQclear(res);
    return group;
}

std::vector<DatabaseManager::Group> DatabaseManager::getUserGroups(const std::string& user_id) {
    std::vector<Group> groups;
    const char* param_values[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("get_user_groups", 1, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            Group group;
            group.id = std::string(PQgetvalue(res, i, 0));
            group.name = std::string(PQgetvalue(res, i, 1));
            group.description = PQgetisnull(res, i, 2) ? "" : std::string(PQgetvalue(res, i, 2));
            group.creator_id = std::string(PQgetvalue(res, i, 3));
            group.invite_link = PQgetisnull(res, i, 4) ? "" : std::string(PQgetvalue(res, i, 4));
            group.created_at = std::string(PQgetvalue(res, i, 5));
            groups.push_back(group);
        }
    }
    
    if (res) PQclear(res);
    return groups;
}

bool DatabaseManager::addGroupMember(const std::string& group_id, const std::string& user_id, const std::string& role) {
    const char* param_values[3] = {group_id.c_str(), user_id.c_str(), role.c_str()};
    PGresult* res = db_->executePrepared("add_group_member", 3, param_values);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool DatabaseManager::sendGroupMessage(const std::string& group_id, const std::string& sender_id, const std::string& content,
                                      const std::string& message_type, const std::string& file_path,
                                      const std::string& file_name, long long file_size,
                                      const std::string& reply_to_message_id,
                                      const std::string& forwarded_from_user_id, const std::string& forwarded_from_username,
                                      const std::string& forwarded_from_message_id) {
    const std::string file_size_str = std::to_string(file_size);
    const char* param_values[11] = {
        group_id.c_str(),
        sender_id.c_str(),
        content.c_str(),
        message_type.c_str(),
        file_path.empty() ? nullptr : file_path.c_str(),
        file_name.empty() ? nullptr : file_name.c_str(),
        file_size_str.c_str(),
        reply_to_message_id.empty() ? nullptr : reply_to_message_id.c_str(),
        forwarded_from_user_id.empty() ? nullptr : forwarded_from_user_id.c_str(),
        forwarded_from_username.empty() ? nullptr : forwarded_from_username.c_str(),
        forwarded_from_message_id.empty() ? nullptr : forwarded_from_message_id.c_str()
    };
    
    PGresult* res = db_->executePrepared("send_group_message", 11, param_values);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

std::vector<DatabaseManager::GroupMessage> DatabaseManager::getGroupMessages(const std::string& group_id, int limit) {
    std::vector<GroupMessage> messages;
    const std::string limit_str = std::to_string(limit);
    const char* param_values[2] = {group_id.c_str(), limit_str.c_str()};
    PGresult* res = db_->executePrepared("get_group_messages", 2, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            GroupMessage msg;
            msg.id = std::string(PQgetvalue(res, i, 0));
            msg.group_id = std::string(PQgetvalue(res, i, 1));
            msg.sender_id = std::string(PQgetvalue(res, i, 2));
            msg.sender_username = std::string(PQgetvalue(res, i, 3));
            msg.content = std::string(PQgetvalue(res, i, 4));
            msg.message_type = std::string(PQgetvalue(res, i, 5));
            msg.file_path = PQgetisnull(res, i, 6) ? "" : std::string(PQgetvalue(res, i, 6));
            msg.file_name = PQgetisnull(res, i, 7) ? "" : std::string(PQgetvalue(res, i, 7));
            msg.file_size = PQgetisnull(res, i, 8) ? 0 : std::stoll(PQgetvalue(res, i, 8));
            msg.reply_to_message_id = PQgetisnull(res, i, 9) ? "" : std::string(PQgetvalue(res, i, 9));
            msg.forwarded_from_user_id = PQgetisnull(res, i, 10) ? "" : std::string(PQgetvalue(res, i, 10));
            msg.forwarded_from_username = PQgetisnull(res, i, 11) ? "" : std::string(PQgetvalue(res, i, 11));
            msg.forwarded_from_message_id = PQgetisnull(res, i, 12) ? "" : std::string(PQgetvalue(res, i, 12));
            msg.is_pinned = PQgetvalue(res, i, 13)[0] == 't';
            msg.created_at = std::string(PQgetvalue(res, i, 14));
            messages.push_back(msg);
        }
    }
    
    if (res) PQclear(res);
    return messages;
}

std::string DatabaseManager::createGroupInviteLink(const std::string& group_id, const std::string& creator_id, int expires_in_seconds) {
    std::string expires_at = "";
    if (expires_in_seconds > 0) {
        std::time_t now = std::time(nullptr);
        std::time_t expires = now + expires_in_seconds;
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::gmtime(&expires));
        expires_at = std::string(buffer);
    }
    
    // Try to generate a unique invite link (max 5 attempts)
    for (int attempt = 0; attempt < 5; attempt++) {
        std::string invite_link = generateInviteLink();
        
        const char* param_values[4] = {
            group_id.c_str(),
            invite_link.c_str(),
            creator_id.c_str(),
            expires_at.empty() ? nullptr : expires_at.c_str()
        };
        
        PGresult* res = db_->executePrepared("create_group_invite", 4, param_values);
        
        if (res && PQresultStatus(res) == PGRES_COMMAND_OK) {
            PQclear(res);
            return invite_link;
        }
        
        // Check if it's a unique constraint violation
        if (res) {
            std::string error = PQresultErrorMessage(res);
            PQclear(res);
            
            // If it's not a unique constraint error, log and return empty
            if (error.find("unique") == std::string::npos && 
                error.find("duplicate") == std::string::npos) {
                Logger::getInstance().error("Failed to create group invite link: " + error);
                return "";
            }
            // Otherwise, try again with a new link
        } else {
            Logger::getInstance().error("Failed to execute create_group_invite prepared statement");
            return "";
        }
    }
    
    Logger::getInstance().error("Failed to create group invite link after 5 attempts (unique constraint conflicts)");
    return "";
}

bool DatabaseManager::updateGroupName(const std::string& group_id, const std::string& new_name) {
    const char* param_values[2] = {new_name.c_str(), group_id.c_str()};
    PGresult* res = db_->executePrepared("update_group_name", 2, param_values);
    
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::updateGroupDescription(const std::string& group_id, const std::string& new_description) {
    const char* param_values[2] = {new_description.c_str(), group_id.c_str()};
    PGresult* res = db_->executePrepared("update_group_description", 2, param_values);
    
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::pinGroupMessage(const std::string& group_id, const std::string& message_id, const std::string& user_id) {
    const char* param_values[3] = {group_id.c_str(), message_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("pin_group_message", 3, param_values);
    
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    
    // Обновляем флаг is_pinned в сообщении
    const char* update_params[1] = {message_id.c_str()};
    PGresult* update_res = db_->executePrepared("update_message_pinned", 1, update_params);
    if (update_res) PQclear(update_res);
    
    return success;
}

// Остальные методы - упрощенные реализации
bool DatabaseManager::removeGroupMember(const std::string& group_id, const std::string& user_id) {
    const char* param_values[2] = {group_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("remove_group_member", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::updateGroupMemberRole(const std::string& group_id, const std::string& user_id, const std::string& role) {
    const char* param_values[3] = {role.c_str(), group_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("update_group_member_role", 3, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::muteGroupMember(const std::string& group_id, const std::string& user_id, bool muted) {
    const char* param_values[3] = {muted ? "true" : "false", group_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("mute_group_member", 3, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::banGroupMember(const std::string& group_id, const std::string& user_id, bool banned, const std::string& until) {
    const char* param_values[4] = {
        banned ? "true" : "false",
        until.empty() ? nullptr : until.c_str(),
        group_id.c_str(),
        user_id.c_str()
    };
    PGresult* res = db_->executePrepared("ban_group_member", 4, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

std::vector<DatabaseManager::GroupMember> DatabaseManager::getGroupMembers(const std::string& group_id) {
    std::vector<GroupMember> members;
    const char* param_values[1] = {group_id.c_str()};
    PGresult* res = db_->executePrepared("get_group_members", 1, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            GroupMember member;
            member.id = std::string(PQgetvalue(res, i, 0));
            member.group_id = std::string(PQgetvalue(res, i, 1));
            member.user_id = std::string(PQgetvalue(res, i, 2));
            member.username = std::string(PQgetvalue(res, i, 3));
            member.role = std::string(PQgetvalue(res, i, 4));
            member.is_muted = PQgetvalue(res, i, 5)[0] == 't';
            member.is_banned = PQgetvalue(res, i, 6)[0] == 't';
            member.joined_at = std::string(PQgetvalue(res, i, 7));
            member.permissions = std::string(PQgetvalue(res, i, 8));
            members.push_back(member);
        }
    }
    
    if (res) PQclear(res);
    return members;
}

DatabaseManager::GroupMember DatabaseManager::getGroupMember(const std::string& group_id, const std::string& user_id) {
    GroupMember member;
    const char* param_values[2] = {group_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("get_group_member", 2, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        member.id = std::string(PQgetvalue(res, 0, 0));
        member.group_id = std::string(PQgetvalue(res, 0, 1));
        member.user_id = std::string(PQgetvalue(res, 0, 2));
        member.username = std::string(PQgetvalue(res, 0, 3));
        member.role = std::string(PQgetvalue(res, 0, 4));
        member.is_muted = PQgetvalue(res, 0, 5)[0] == 't';
        member.is_banned = PQgetvalue(res, 0, 6)[0] == 't';
        member.joined_at = std::string(PQgetvalue(res, 0, 7));
        member.permissions = std::string(PQgetvalue(res, 0, 8));
    }
    
    if (res) PQclear(res);
    return member;
}

bool DatabaseManager::updateAdminPermissions(const std::string& group_id, const std::string& user_id, const std::string& permissions_json) {
    const char* param_values[3] = {permissions_json.c_str(), group_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("update_admin_permissions", 3, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

std::string DatabaseManager::joinGroupByInviteLink(const std::string& invite_link, const std::string& user_id) {
    // Проверяем валидность ссылки
    const char* param_values[1] = {invite_link.c_str()};
    PGresult* res = db_->executePrepared("get_invite_info", 1, param_values);
    
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return "";
    }
    
    std::string group_id = std::string(PQgetvalue(res, 0, 0));
    // Проверяем срок действия и количество использований
    // ... (упрощено)
    
    PQclear(res);
    
    // Добавляем пользователя в группу
    if (addGroupMember(group_id, user_id, "member")) {
        return group_id;
    }
    return "";
}

bool DatabaseManager::unpinGroupMessage(const std::string& group_id, const std::string& message_id) {
    const char* param_values[2] = {group_id.c_str(), message_id.c_str()};
    PGresult* res = db_->executePrepared("unpin_group_message", 2, param_values);
    
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    
    // Обновляем флаг is_pinned
    const char* update_params[1] = {message_id.c_str()};
    PGresult* update_res = db_->executePrepared("update_message_unpinned", 1, update_params);
    if (update_res) PQclear(update_res);
    
    return success;
}

bool DatabaseManager::getGroupMessageMeta(const std::string& message_id, std::string& out_group_id, std::string& out_sender_id) {
    const char* params[1] = {message_id.c_str()};
    PGresult* res = db_->executePrepared("get_group_message_meta", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_group_id = PQgetvalue(res, 0, 0);
    out_sender_id = PQgetvalue(res, 0, 1);
    PQclear(res);
    return true;
}

bool DatabaseManager::deleteGroupMessage(const std::string& group_id, const std::string& message_id) {
    const char* params[2] = {message_id.c_str(), group_id.c_str()};
    PGresult* res = db_->executePrepared("delete_group_message", 2, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK) && std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::deleteGroup(const std::string& group_id) {
    // Удаляем все связанные данные
    const char* params[1] = {group_id.c_str()};
    
    PGresult* res = db_->executePrepared("delete_group_messages", 1, params);
    if (res) PQclear(res);
    
    res = db_->executePrepared("delete_group_members", 1, params);
    if (res) PQclear(res);
    
    res = db_->executePrepared("delete_group_invites", 1, params);
    if (res) PQclear(res);
    
    res = db_->executePrepared("delete_group_pinned", 1, params);
    if (res) PQclear(res);
    
    // Удаляем саму группу
    res = db_->executePrepared("delete_group", 1, params);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    
    return success;
}

std::vector<DatabaseManager::GroupMessage> DatabaseManager::searchGroupMessages(const std::string& group_id, const std::string& query_text, int limit) {
    std::vector<GroupMessage> messages;
    
    std::string search_pattern = "%" + query_text + "%";
    std::string limit_str = std::to_string(limit);
    
    const char* params[3] = {group_id.c_str(), search_pattern.c_str(), limit_str.c_str()};
    PGresult* res = db_->executePrepared("search_group_messages", 3, params);
    
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
            msg.file_size = std::stoll(PQgetvalue(res, i, 8));
            msg.created_at = std::string(PQgetvalue(res, i, 9));
            msg.is_pinned = (std::string(PQgetvalue(res, i, 10)) == "t");
            messages.push_back(msg);
        }
    }
    
    if (res) PQclear(res);
    return messages;
}

bool DatabaseManager::updateGroupPermission(const std::string& group_id, const std::string& permission, bool enabled) {
    // Обновляем разрешения в JSON-поле группы
    std::string enabled_str = enabled ? "true" : "false";
    
    // Формируем JSON для обновления
    std::string json_update = "{\"" + permission + "\":" + enabled_str + "}";
    
    const char* params[2] = {json_update.c_str(), group_id.c_str()};
    PGresult* res = db_->executePrepared("update_group_permissions", 2, params);
    
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    
    return success;
}

// ============================================
// Public Directory Methods
// ============================================

std::vector<DatabaseManager::Group> DatabaseManager::getPublicGroups(const std::string& category, const std::string& search, int limit) {
    std::vector<Group> groups;
    
    // Build query dynamically based on parameters
    std::string query = "SELECT g.id, g.name, g.description, g.creator_id, g.invite_link, g.created_at, "
                        "COALESCE(g.category, '') as category, COALESCE(g.is_public, false) as is_public "
                        "FROM groups g WHERE COALESCE(g.is_public, false) = true";
    
    std::vector<std::string> params_vec;
    int param_num = 1;
    
    if (!category.empty() && category != "all" && category != "trending") {
        query += " AND LOWER(COALESCE(g.category, '')) = LOWER($" + std::to_string(param_num++) + ")";
        params_vec.push_back(category);
    }
    
    if (!search.empty()) {
        query += " AND (LOWER(g.name) LIKE LOWER($" + std::to_string(param_num) + ") OR LOWER(COALESCE(g.description, '')) LIKE LOWER($" + std::to_string(param_num) + "))";
        params_vec.push_back("%" + search + "%");
        param_num++;
    }
    
    query += " ORDER BY g.created_at DESC LIMIT " + std::to_string(limit);
    
    // Execute query
    std::vector<const char*> params;
    for (const auto& p : params_vec) {
        params.push_back(p.c_str());
    }
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 
                                  static_cast<int>(params.size()), nullptr, 
                                  params.empty() ? nullptr : params.data(), 
                                  nullptr, nullptr, 0);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            Group group;
            group.id = PQgetisnull(res, i, 0) ? "" : std::string(PQgetvalue(res, i, 0));
            group.name = PQgetisnull(res, i, 1) ? "" : std::string(PQgetvalue(res, i, 1));
            group.description = PQgetisnull(res, i, 2) ? "" : std::string(PQgetvalue(res, i, 2));
            group.creator_id = PQgetisnull(res, i, 3) ? "" : std::string(PQgetvalue(res, i, 3));
            group.invite_link = PQgetisnull(res, i, 4) ? "" : std::string(PQgetvalue(res, i, 4));
            group.created_at = PQgetisnull(res, i, 5) ? "" : std::string(PQgetvalue(res, i, 5));
            group.category = PQgetisnull(res, i, 6) ? "" : std::string(PQgetvalue(res, i, 6));
            group.is_public = !PQgetisnull(res, i, 7) && std::string(PQgetvalue(res, i, 7)) == "t";
            groups.push_back(group);
        }
    }
    
    if (res) PQclear(res);
    return groups;
}

int DatabaseManager::getGroupMembersCount(const std::string& group_id) {
    const char* params[1] = {group_id.c_str()};
    std::string query = "SELECT COUNT(*) FROM group_members WHERE group_id = $1 AND is_banned = false";
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 1, nullptr, params, nullptr, nullptr, 0);
    
    int count = 0;
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        try {
            count = std::stoi(PQgetvalue(res, 0, 0));
        } catch (...) {}
    }
    
    if (res) PQclear(res);
    return count;
}

bool DatabaseManager::isGroupMember(const std::string& group_id, const std::string& user_id) {
    const char* params[2] = {group_id.c_str(), user_id.c_str()};
    std::string query = "SELECT 1 FROM group_members WHERE group_id = $1 AND user_id = $2 AND is_banned = false LIMIT 1";
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 2, nullptr, params, nullptr, nullptr, 0);
    
    bool is_member = (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0);
    
    if (res) PQclear(res);
    return is_member;
}

bool DatabaseManager::setGroupPublic(const std::string& group_id, bool is_public, const std::string& category) {
    std::string is_public_str = is_public ? "TRUE" : "FALSE";
    std::string query = "UPDATE groups SET is_public = " + is_public_str + ", category = $1 WHERE id = $2";
    
    const char* params[2] = {category.c_str(), group_id.c_str()};
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 2, nullptr, params, nullptr, nullptr, 0);
    
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    
    return success;
}

} // namespace xipher

