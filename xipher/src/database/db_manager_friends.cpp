#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cstdlib>

namespace xipher {

std::vector<Friend> DatabaseManager::getFriends(const std::string& user_id) {
    std::vector<Friend> friends;
    
    const char* param_values[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("get_friends", 1, param_values);
    if (!res) {
        return friends;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        Friend friend_;
        friend_.id = PQgetvalue(res, i, 0);
        friend_.user_id = friend_.id;
        friend_.username = PQgetvalue(res, i, 1);
        friend_.avatar_url = PQgetisnull(res, i, 2) ? "" : std::string(PQgetvalue(res, i, 2));
        friend_.created_at = PQgetvalue(res, i, 3);
        friends.push_back(friend_);
    }
    
    PQclear(res);
    return friends;
}

bool DatabaseManager::sendMessage(const std::string& sender_id, const std::string& receiver_id, const std::string& content,
                                  const std::string& message_type, const std::string& file_path,
                                  const std::string& file_name, long long file_size, const std::string& reply_to_message_id,
                                  const std::string& forwarded_from_user_id, const std::string& forwarded_from_username,
                                  const std::string& forwarded_from_message_id, const std::string& reply_markup) {
    // Проверяем, что все обязательные параметры не пустые
    if (sender_id.empty() || receiver_id.empty() || content.empty()) {
        Logger::getInstance().error("DatabaseManager::sendMessage - Empty required parameters: sender_id=" + sender_id + 
                                   ", receiver_id=" + receiver_id + ", content length=" + std::to_string(content.length()));
        return false;
    }
    
    // Для избранных сообщений (sender_id == receiver_id) убеждаемся, что
    // возможна вставка self-message даже если старая БД все еще содержит constraint.
    if (sender_id == receiver_id) {
        PGresult* res = db_->executeQuery("ALTER TABLE messages DROP CONSTRAINT IF EXISTS no_self_message");
        if (!res) {
            Logger::getInstance().warning("Failed to drop no_self_message constraint before self-message: " + db_->getLastError());
        } else {
            PQclear(res);
            Logger::getInstance().info("no_self_message constraint dropped (or absent) before self-message insert");
        }
    }

    std::string file_size_str = std::to_string(file_size);
    const char* param_values[9] = {
        sender_id.c_str(), 
        receiver_id.c_str(), 
        content.c_str(),
        message_type.c_str(),
        file_path.empty() ? nullptr : file_path.c_str(),
        file_name.empty() ? nullptr : file_name.c_str(),
        file_size_str.c_str(),
        reply_to_message_id.empty() ? nullptr : reply_to_message_id.c_str(),
        reply_markup.empty() ? nullptr : reply_markup.c_str()
    };
    
    // Логируем для отладки
    Logger::getInstance().info("DatabaseManager::sendMessage - sender_id: " + sender_id + ", receiver_id: " + receiver_id + 
                               ", content length: " + std::to_string(content.length()) + 
                               ", is_saved_messages: " + (sender_id == receiver_id ? "true" : "false") +
                               ", reply_markup: " + (reply_markup.empty() ? "none" : "present"));
    
    PGresult* res = db_->executePrepared("send_message", 9, param_values);
    if (!res) {
        Logger::getInstance().error("Failed to execute send_message prepared statement - sender_id: " + sender_id + ", receiver_id: " + receiver_id);
        return false;
    }
    
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    if (!success) {
        std::string error_msg = PQresultErrorMessage(res) ? PQresultErrorMessage(res) : "Unknown error";
        Logger::getInstance().error("send_message query failed - sender_id: " + sender_id + ", receiver_id: " + receiver_id + ", error: " + error_msg);
        // Логируем детали для отладки saved messages
        if (sender_id == receiver_id) {
            Logger::getInstance().error("This is a saved message (sender_id == receiver_id). Error details: " + error_msg);
        }
    } else {
        Logger::getInstance().info("Message sent successfully - sender_id: " + sender_id + ", receiver_id: " + receiver_id + 
                                  ", is_saved_messages: " + (sender_id == receiver_id ? "true" : "false"));
    }
    PQclear(res);
    return success;
}

std::vector<Message> DatabaseManager::getMessages(const std::string& user1_id, const std::string& user2_id, int limit) {
    std::vector<Message> messages;
    
    std::string limit_str = std::to_string(limit);
    PGresult* res = nullptr;
    
    // Если это избранные сообщения (user1_id == user2_id), используем специальный запрос
    if (user1_id == user2_id) {
        const char* param_values[2] = {user1_id.c_str(), limit_str.c_str()};
        res = db_->executePrepared("get_saved_messages", 2, param_values);
    } else {
        const char* param_values[3] = {user1_id.c_str(), user2_id.c_str(), limit_str.c_str()};
        res = db_->executePrepared("get_messages", 3, param_values);
    }
    
    if (!res) {
        return messages;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        Message msg;
        msg.id = PQgetvalue(res, i, 0);
        msg.sender_id = PQgetvalue(res, i, 1);
        msg.receiver_id = PQgetvalue(res, i, 2);
        msg.content = PQgetvalue(res, i, 3);
        msg.created_at = PQgetvalue(res, i, 4);
        msg.is_read = (strcmp(PQgetvalue(res, i, 5), "t") == 0);
        msg.is_delivered = (strcmp(PQgetvalue(res, i, 6), "t") == 0);
        msg.message_type = (PQgetisnull(res, i, 7)) ? "text" : std::string(PQgetvalue(res, i, 7));
        msg.file_path = (PQgetisnull(res, i, 8)) ? "" : std::string(PQgetvalue(res, i, 8));
        msg.file_name = (PQgetisnull(res, i, 9)) ? "" : std::string(PQgetvalue(res, i, 9));
        msg.file_size = (PQgetisnull(res, i, 10)) ? 0 : std::stoll(PQgetvalue(res, i, 10));
        msg.reply_to_message_id = (PQgetisnull(res, i, 11)) ? "" : std::string(PQgetvalue(res, i, 11));
        msg.reply_markup = (PQgetisnull(res, i, 12)) ? "" : std::string(PQgetvalue(res, i, 12));
        msg.is_pinned = (PQgetisnull(res, i, 13)) ? false : (PQgetvalue(res, i, 13)[0] == 't');
        messages.push_back(msg);
    }
    
    PQclear(res);
    return messages;
}

bool DatabaseManager::pinDirectMessage(const std::string& user1_id, const std::string& user2_id, const std::string& message_id, const std::string& pinned_by) {
    const char* params[4] = {user1_id.c_str(), user2_id.c_str(), message_id.c_str(), pinned_by.c_str()};
    PGresult* res = db_->executePrepared("pin_direct_message", 4, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::unpinDirectMessage(const std::string& user1_id, const std::string& user2_id) {
    const char* params[2] = {user1_id.c_str(), user2_id.c_str()};
    PGresult* res = db_->executePrepared("unpin_direct_message", 2, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return ok;
}

bool DatabaseManager::getDirectPinnedMessage(const std::string& user1_id, const std::string& user2_id, std::string& out_message_id) {
    const char* params[2] = {user1_id.c_str(), user2_id.c_str()};
    PGresult* res = db_->executePrepared("get_direct_pinned_message", 2, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_message_id = PQgetvalue(res, 0, 0);
    PQclear(res);
    return true;
}

Message DatabaseManager::getLastMessage(const std::string& user1_id, const std::string& user2_id) {
    Message msg;
    PGresult* res = nullptr;
    
    // Если это избранные сообщения (user1_id == user2_id), используем специальный запрос
    if (user1_id == user2_id) {
        const char* param_values[1] = {user1_id.c_str()};
        res = db_->executePrepared("get_last_saved_message", 1, param_values);
    } else {
        const char* param_values[2] = {user1_id.c_str(), user2_id.c_str()};
        res = db_->executePrepared("get_last_message", 2, param_values);
    }
    
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return msg;
    }
    
    msg.id = PQgetvalue(res, 0, 0);
    msg.sender_id = PQgetvalue(res, 0, 1);
    msg.receiver_id = PQgetvalue(res, 0, 2);
    msg.content = PQgetvalue(res, 0, 3);
    msg.created_at = PQgetvalue(res, 0, 4);
    msg.is_read = (strcmp(PQgetvalue(res, 0, 5), "t") == 0);
    msg.is_delivered = (strcmp(PQgetvalue(res, 0, 6), "t") == 0);
    msg.message_type = (PQgetisnull(res, 0, 7)) ? "text" : std::string(PQgetvalue(res, 0, 7));
    msg.file_path = (PQgetisnull(res, 0, 8)) ? "" : std::string(PQgetvalue(res, 0, 8));
    msg.file_name = (PQgetisnull(res, 0, 9)) ? "" : std::string(PQgetvalue(res, 0, 9));
    msg.file_size = (PQgetisnull(res, 0, 10)) ? 0 : std::stoll(PQgetvalue(res, 0, 10));
    msg.reply_to_message_id = (PQgetisnull(res, 0, 11)) ? "" : std::string(PQgetvalue(res, 0, 11));
    msg.reply_markup = (PQgetisnull(res, 0, 12)) ? "" : std::string(PQgetvalue(res, 0, 12));
    
    PQclear(res);
    return msg;
}

Message DatabaseManager::getMessageById(const std::string& message_id) {
    Message msg;
    const char* params[1] = { message_id.c_str() };
    PGresult* res = db_->executePrepared("get_message_by_id", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return msg;
    }

    msg.id = PQgetvalue(res, 0, 0);
    msg.sender_id = PQgetvalue(res, 0, 1);
    msg.receiver_id = PQgetvalue(res, 0, 2);
    msg.content = PQgetvalue(res, 0, 3);
    msg.created_at = PQgetvalue(res, 0, 4);
    msg.is_read = (strcmp(PQgetvalue(res, 0, 5), "t") == 0);
    msg.is_delivered = (strcmp(PQgetvalue(res, 0, 6), "t") == 0);
    msg.message_type = (PQgetisnull(res, 0, 7)) ? "text" : std::string(PQgetvalue(res, 0, 7));
    msg.file_path = (PQgetisnull(res, 0, 8)) ? "" : std::string(PQgetvalue(res, 0, 8));
    msg.file_name = (PQgetisnull(res, 0, 9)) ? "" : std::string(PQgetvalue(res, 0, 9));
    msg.file_size = (PQgetisnull(res, 0, 10)) ? 0 : std::stoll(PQgetvalue(res, 0, 10));
    msg.reply_to_message_id = (PQgetisnull(res, 0, 11)) ? "" : std::string(PQgetvalue(res, 0, 11));
    msg.reply_markup = (PQgetisnull(res, 0, 12)) ? "" : std::string(PQgetvalue(res, 0, 12));

    PQclear(res);
    return msg;
}

bool DatabaseManager::deleteMessageById(const std::string& message_id) {
    const char* params[1] = { message_id.c_str() };
    PGresult* res = db_->executePrepared("delete_message_by_id", 1, params);
    if (!res) {
        return false;
    }

    bool success = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    return success;
}

int DatabaseManager::getUnreadCount(const std::string& user_id, const std::string& sender_id) {
    const char* param_values[2] = {user_id.c_str(), sender_id.c_str()};
    PGresult* res = db_->executePrepared("get_unread_count", 2, param_values);
    if (!res) {
        return 0;
    }
    
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

bool DatabaseManager::markMessagesAsRead(const std::string& user_id, const std::string& sender_id) {
    const char* param_values[2] = {user_id.c_str(), sender_id.c_str()};
    PGresult* res = db_->executePrepared("mark_messages_read", 2, param_values);
    if (!res) {
        return false;
    }
    
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

bool DatabaseManager::markMessagesAsDelivered(const std::string& user_id, const std::string& sender_id) {
    const char* param_values[2] = {user_id.c_str(), sender_id.c_str()};
    PGresult* res = db_->executePrepared("mark_messages_delivered", 2, param_values);
    if (!res) {
        return false;
    }

    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

bool DatabaseManager::markMessageDeliveredById(const std::string& message_id) {
    const char* params[1] = {message_id.c_str()};
    PGresult* res = db_->executePrepared("mark_message_delivered_by_id", 1, params);
    if (!res) {
        return false;
    }

    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

std::vector<Friend> DatabaseManager::getChatPartners(const std::string& user_id) {
    std::vector<Friend> partners;
    
    const char* param_values[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("get_chat_partners", 1, param_values);
    if (!res) {
        return partners;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        Friend partner;
        partner.id = PQgetvalue(res, i, 0);
        partner.user_id = partner.id;
        partner.username = PQgetvalue(res, i, 1);
        partner.avatar_url = PQgetisnull(res, i, 2) ? "" : std::string(PQgetvalue(res, i, 2));
        partner.created_at = (PQgetisnull(res, i, 3)) ? "" : std::string(PQgetvalue(res, i, 3));
        partners.push_back(partner);
    }
    
    PQclear(res);
    return partners;
}

bool DatabaseManager::hasSavedMessages(const std::string& user_id) {
    const char* param_values[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("has_saved_messages", 1, param_values);
    if (!res) {
        return false;
    }
    
    bool has_messages = (strcmp(PQgetvalue(res, 0, 0), "t") == 0);
    PQclear(res);
    return has_messages;
}

std::vector<Message> DatabaseManager::getMessageContext(const std::string& user1_id,
                                                       const std::string& user2_id,
                                                       const std::string& message_id,
                                                       int limit) {
    std::vector<Message> messages;
    if (limit <= 0) {
        return messages;
    }

    const std::string limitStr = std::to_string(limit);
    const char* params[4] = {user1_id.c_str(), user2_id.c_str(), message_id.c_str(), limitStr.c_str()};
    PGresult* res = db_->executePrepared("get_message_context", 4, params);
    if (!res) {
        return messages;
    }

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        Message msg;
        msg.id = PQgetvalue(res, i, 0);
        msg.sender_id = PQgetvalue(res, i, 1);
        msg.receiver_id = PQgetvalue(res, i, 2);
        msg.content = PQgetvalue(res, i, 3);
        msg.created_at = PQgetvalue(res, i, 4);
        msg.is_read = (strcmp(PQgetvalue(res, i, 5), "t") == 0);
        msg.is_delivered = (strcmp(PQgetvalue(res, i, 6), "t") == 0);
        msg.message_type = (PQgetisnull(res, i, 7)) ? "text" : std::string(PQgetvalue(res, i, 7));
        msg.file_path = (PQgetisnull(res, i, 8)) ? "" : std::string(PQgetvalue(res, i, 8));
        msg.file_name = (PQgetisnull(res, i, 9)) ? "" : std::string(PQgetvalue(res, i, 9));
        msg.file_size = (PQgetisnull(res, i, 10)) ? 0 : std::stoll(PQgetvalue(res, i, 10));
        msg.reply_to_message_id = (PQgetisnull(res, i, 11)) ? "" : std::string(PQgetvalue(res, i, 11));
        messages.push_back(msg);
    }

    PQclear(res);
    return messages;
}

bool DatabaseManager::createReport(const std::string& reporter_id,
                                   const std::string& reported_user_id,
                                   const std::string& message_id,
                                   const std::string& message_type,
                                   const std::string& message_content,
                                   const std::string& reason,
                                   const std::string& comment,
                                   const std::string& context_json,
                                   std::string& out_report_id) {
    const char* params[8] = {
        reporter_id.c_str(),
        reported_user_id.c_str(),
        message_id.c_str(),
        message_type.c_str(),
        message_content.c_str(),
        reason.c_str(),
        comment.c_str(),
        context_json.c_str()
    };
    PGresult* res = db_->executePrepared("create_message_report", 8, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_report_id = PQgetvalue(res, 0, 0);
    PQclear(res);
    return true;
}

std::vector<Report> DatabaseManager::getReportsPaginated(int page,
                                                         int page_size,
                                                         const std::string& status_filter) {
    std::vector<Report> reports;
    if (page < 1) page = 1;
    if (page_size < 1) page_size = 10;

    const std::string limitStr = std::to_string(page_size);
    const std::string offsetStr = std::to_string((page - 1) * page_size);
    const char* params[3] = {limitStr.c_str(), offsetStr.c_str(), status_filter.c_str()};
    PGresult* res = db_->executePrepared("get_message_reports_paginated", 3, params);
    if (!res) {
        return reports;
    }

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        Report r;
        r.id = PQgetvalue(res, i, 0);
        r.reporter_id = PQgetvalue(res, i, 1);
        r.reporter_username = PQgetvalue(res, i, 2);
        r.reported_user_id = PQgetvalue(res, i, 3);
        r.reported_username = PQgetvalue(res, i, 4);
        r.message_id = PQgetvalue(res, i, 5);
        r.message_type = PQgetisnull(res, i, 6) ? "direct" : std::string(PQgetvalue(res, i, 6));
        r.message_content = PQgetvalue(res, i, 7);
        r.reason = PQgetvalue(res, i, 8);
        r.comment = PQgetvalue(res, i, 9);
        r.context_json = PQgetisnull(res, i, 10) ? "[]" : std::string(PQgetvalue(res, i, 10));
        r.status = PQgetvalue(res, i, 11);
        r.resolution_note = PQgetvalue(res, i, 12);
        r.resolved_by = PQgetisnull(res, i, 13) ? "" : std::string(PQgetvalue(res, i, 13));
        r.resolved_at = PQgetisnull(res, i, 14) ? "" : std::string(PQgetvalue(res, i, 14));
        r.created_at = PQgetvalue(res, i, 15);
        reports.push_back(r);
    }

    PQclear(res);
    return reports;
}

long long DatabaseManager::countReports(const std::string& status_filter) {
    const char* params[1] = {status_filter.c_str()};
    PGresult* res = db_->executePrepared("count_message_reports", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    long long count = std::atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

Report DatabaseManager::getReportById(const std::string& report_id) {
    Report r;
    const char* params[1] = {report_id.c_str()};
    PGresult* res = db_->executePrepared("get_message_report_by_id", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return r;
    }

    r.id = PQgetvalue(res, 0, 0);
    r.reporter_id = PQgetvalue(res, 0, 1);
    r.reporter_username = PQgetvalue(res, 0, 2);
    r.reported_user_id = PQgetvalue(res, 0, 3);
    r.reported_username = PQgetvalue(res, 0, 4);
    r.message_id = PQgetvalue(res, 0, 5);
    r.message_type = PQgetisnull(res, 0, 6) ? "direct" : std::string(PQgetvalue(res, 0, 6));
    r.message_content = PQgetvalue(res, 0, 7);
    r.reason = PQgetvalue(res, 0, 8);
    r.comment = PQgetvalue(res, 0, 9);
    r.context_json = PQgetisnull(res, 0, 10) ? "[]" : std::string(PQgetvalue(res, 0, 10));
    r.status = PQgetvalue(res, 0, 11);
    r.resolution_note = PQgetvalue(res, 0, 12);
    r.resolved_by = PQgetisnull(res, 0, 13) ? "" : std::string(PQgetvalue(res, 0, 13));
    r.resolved_at = PQgetisnull(res, 0, 14) ? "" : std::string(PQgetvalue(res, 0, 14));
    r.created_at = PQgetvalue(res, 0, 15);

    PQclear(res);
    return r;
}

bool DatabaseManager::updateReportStatus(const std::string& report_id,
                                         const std::string& status,
                                         const std::string& resolved_by,
                                         const std::string& resolution_note) {
    const char* params[4] = {
        report_id.c_str(),
        status.c_str(),
        resolved_by.c_str(),
        resolution_note.c_str()
    };
    PGresult* res = db_->executePrepared("update_message_report_status", 4, params);
    if (!res) {
        return false;
    }
    bool ok = true;
    PQclear(res);
    return ok;
}

bool DatabaseManager::setUserRole(const std::string& user_id, const std::string& role) {
    const char* params[2] = {user_id.c_str(), role.c_str()};
    PGresult* res = db_->executePrepared("set_user_role", 2, params);
    if (!res) {
        return false;
    }
    bool ok = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    return ok;
}

bool DatabaseManager::setUserAdminFlag(const std::string& user_id, bool is_admin) {
    const std::string adminStr = is_admin ? "true" : "false";
    const char* params[2] = {user_id.c_str(), adminStr.c_str()};
    PGresult* res = db_->executePrepared("set_user_admin_flag", 2, params);
    if (!res) {
        return false;
    }
    bool ok = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    return ok;
}

bool DatabaseManager::deleteUserSessionsByUser(const std::string& user_id) {
    const char* params[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("delete_user_sessions_by_user", 1, params);
    if (!res) {
        return false;
    }
    bool ok = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    return ok;
}

long long DatabaseManager::countBannedUsers() {
    PGresult* res = db_->executePrepared("count_banned_users", 0, nullptr);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    long long count = std::atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

long long DatabaseManager::countActiveUsersSince(int seconds) {
    const std::string secondsStr = std::to_string(seconds);
    const char* params[1] = {secondsStr.c_str()};
    PGresult* res = db_->executePrepared("count_active_users_since", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    long long count = std::atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

long long DatabaseManager::countGroups() {
    PGresult* res = db_->executePrepared("count_groups", 0, nullptr);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    long long count = std::atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

long long DatabaseManager::countChannels() {
    PGresult* res = db_->executePrepared("count_channels", 0, nullptr);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    long long count = std::atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

long long DatabaseManager::countBots() {
    PGresult* res = db_->executePrepared("count_bots", 0, nullptr);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    long long count = std::atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

long long DatabaseManager::countReportsByStatus(const std::string& status_filter) {
    const char* params[1] = {status_filter.c_str()};
    PGresult* res = db_->executePrepared("count_reports_by_status", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    long long count = std::atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

long long DatabaseManager::countReportsForUserWindow(const std::string& reported_user_id,
                                                     const std::string& reason,
                                                     int window_minutes) {
    if (reported_user_id.empty()) {
        return 0;
    }
    if (window_minutes < 1) {
        window_minutes = 60;
    }
    const std::string windowStr = std::to_string(window_minutes);
    const char* params[3] = {reported_user_id.c_str(), reason.c_str(), windowStr.c_str()};
    PGresult* res = db_->executePrepared("count_reports_for_user_window", 3, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return 0;
    }
    long long count = std::atoll(PQgetvalue(res, 0, 0));
    PQclear(res);
    return count;
}

std::string DatabaseManager::getAdminSetting(const std::string& key) {
    const char* params[1] = {key.c_str()};
    PGresult* res = db_->executePrepared("get_admin_setting", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return "";
    }
    std::string value = PQgetvalue(res, 0, 0);
    PQclear(res);
    return value;
}

bool DatabaseManager::upsertAdminSetting(const std::string& key, const std::string& value) {
    const char* params[2] = {key.c_str(), value.c_str()};
    PGresult* res = db_->executePrepared("upsert_admin_setting", 2, params);
    if (!res) {
        return false;
    }
    bool ok = PQresultStatus(res) == PGRES_COMMAND_OK;
    PQclear(res);
    return ok;
}

std::vector<AdminUserSummary> DatabaseManager::searchUsers(const std::string& query, int limit) {
    std::vector<AdminUserSummary> users;
    if (query.empty()) {
        return users;
    }
    if (limit < 1) limit = 20;
    std::string pattern = "%" + query + "%";
    std::string limitStr = std::to_string(limit);
    const char* params[4] = {pattern.c_str(), query.c_str(), query.c_str(), limitStr.c_str()};
    PGresult* res = db_->executePrepared("admin_search_users", 4, params);
    if (!res) {
        return users;
    }
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        AdminUserSummary u;
        u.id = PQgetvalue(res, i, 0);
        u.username = PQgetvalue(res, i, 1);
        u.is_active = PQgetvalue(res, i, 2)[0] == 't';
        u.is_admin = PQgetvalue(res, i, 3)[0] == 't';
        u.is_bot = PQgetvalue(res, i, 4)[0] == 't';
        u.role = PQgetvalue(res, i, 5);
        u.last_login = PQgetvalue(res, i, 6);
        u.last_activity = PQgetvalue(res, i, 7);
        u.created_at = PQgetvalue(res, i, 8);
        users.push_back(u);
    }
    PQclear(res);
    return users;
}

std::vector<AdminMessageSummary> DatabaseManager::searchMessages(const std::string& query, int limit) {
    std::vector<AdminMessageSummary> messages;
    if (query.empty()) {
        return messages;
    }
    if (limit < 1) limit = 20;
    std::string pattern = "%" + query + "%";
    std::string limitStr = std::to_string(limit);
    const char* params[3] = {pattern.c_str(), query.c_str(), limitStr.c_str()};
    PGresult* res = db_->executePrepared("admin_search_messages", 3, params);
    if (!res) {
        return messages;
    }
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        AdminMessageSummary m;
        m.id = PQgetvalue(res, i, 0);
        m.sender_id = PQgetvalue(res, i, 1);
        m.sender_username = PQgetvalue(res, i, 2);
        m.receiver_id = PQgetvalue(res, i, 3);
        m.receiver_username = PQgetvalue(res, i, 4);
        m.content = PQgetvalue(res, i, 5);
        m.message_type = PQgetvalue(res, i, 6);
        m.created_at = PQgetvalue(res, i, 7);
        messages.push_back(m);
    }
    PQclear(res);
    return messages;
}

std::vector<DatabaseManager::Group> DatabaseManager::searchGroups(const std::string& query, int limit) {
    std::vector<Group> groups;
    if (query.empty()) {
        return groups;
    }
    if (limit < 1) limit = 20;
    std::string pattern = "%" + query + "%";
    std::string limitStr = std::to_string(limit);
    const char* params[3] = {pattern.c_str(), query.c_str(), limitStr.c_str()};
    PGresult* res = db_->executePrepared("admin_search_groups", 3, params);
    if (!res) {
        return groups;
    }
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        Group g;
        g.id = PQgetvalue(res, i, 0);
        g.name = PQgetvalue(res, i, 1);
        g.description = PQgetvalue(res, i, 2);
        g.creator_id = PQgetvalue(res, i, 3);
        g.invite_link = PQgetvalue(res, i, 4);
        g.created_at = PQgetvalue(res, i, 5);
        groups.push_back(g);
    }
    PQclear(res);
    return groups;
}

std::vector<DatabaseManager::Channel> DatabaseManager::searchChannels(const std::string& query, int limit) {
    std::vector<Channel> channels;
    if (query.empty()) {
        return channels;
    }
    if (limit < 1) limit = 20;
    std::string pattern = "%" + query + "%";
    std::string limitStr = std::to_string(limit);
    const char* params[3] = {pattern.c_str(), query.c_str(), limitStr.c_str()};
    PGresult* res = db_->executePrepared("admin_search_channels", 3, params);
    if (!res) {
        return channels;
    }
    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        Channel c;
        c.id = PQgetvalue(res, i, 0);
        c.name = PQgetvalue(res, i, 1);
        c.description = PQgetvalue(res, i, 2);
        c.creator_id = PQgetvalue(res, i, 3);
        c.custom_link = PQgetvalue(res, i, 4);
        c.created_at = PQgetvalue(res, i, 5);
        channels.push_back(c);
    }
    PQclear(res);
    return channels;
}

} // namespace xipher
