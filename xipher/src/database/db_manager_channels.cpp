#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>
#include <cstring>
#include <map>

namespace xipher {

bool DatabaseManager::createChannel(const std::string& name, const std::string& description, const std::string& creator_id, std::string& channel_id, const std::string& custom_link) {
    // Проверяем уникальность custom_link если он указан
    if (!custom_link.empty()) {
        if (checkChannelCustomLinkExists(custom_link)) {
            Logger::getInstance().error("Channel custom_link already exists: " + custom_link);
            return false;
        }
    }
    
    const char* param_values[4] = {name.c_str(), description.c_str(), creator_id.c_str(), custom_link.empty() ? nullptr : custom_link.c_str()};
    PGresult* res = db_->executePrepared("create_channel", 4, param_values);
    
    if (!res) {
        Logger::getInstance().error("Failed to execute create_channel prepared statement");
        return false;
    }
    
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        Logger::getInstance().error("create_channel failed: " + std::string(PQresultErrorMessage(res)));
        PQclear(res);
        return false;
    }
    
    if (PQntuples(res) == 0) {
        Logger::getInstance().error("create_channel returned no rows");
        PQclear(res);
        return false;
    }
    
    channel_id = std::string(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    // Добавляем создателя как админа
    if (!addChannelMember(channel_id, creator_id, "creator")) {
        Logger::getInstance().warning("Failed to add creator as channel member, but channel was created");
    }
    
    Logger::getInstance().info("Channel created successfully: " + channel_id);
    return true;
}

bool DatabaseManager::checkChannelCustomLinkExists(const std::string& custom_link) {
    if (custom_link.empty()) {
        return false;
    }
    
    const char* param_values[1] = {custom_link.c_str()};
    PGresult* res = db_->executePrepared("check_channel_custom_link", 1, param_values);
    
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        if (res) PQclear(res);
        return false;
    }
    
    int count = std::stoi(std::string(PQgetvalue(res, 0, 0)));
    PQclear(res);
    
    return count > 0;
}

DatabaseManager::Channel DatabaseManager::getChannelById(const std::string& channel_id) {
    Channel channel;
    const char* param_values[1] = {channel_id.c_str()};
    PGresult* res = db_->executePrepared("get_channel_by_id", 1, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        channel.id = std::string(PQgetvalue(res, 0, 0));
        channel.name = std::string(PQgetvalue(res, 0, 1));
        channel.description = PQgetisnull(res, 0, 2) ? "" : std::string(PQgetvalue(res, 0, 2));
        channel.creator_id = std::string(PQgetvalue(res, 0, 3));
        channel.custom_link = PQgetisnull(res, 0, 4) ? "" : std::string(PQgetvalue(res, 0, 4));
        channel.avatar_url = PQgetisnull(res, 0, 5) ? "" : std::string(PQgetvalue(res, 0, 5));
        channel.is_private = PQgetvalue(res, 0, 6)[0] == 't';
        channel.show_author = PQgetvalue(res, 0, 7)[0] == 't';
        channel.created_at = std::string(PQgetvalue(res, 0, 8));
        channel.is_verified = !PQgetisnull(res, 0, 9) && PQgetvalue(res, 0, 9)[0] == 't';
    }
    
    if (res) PQclear(res);
    return channel;
}

DatabaseManager::Channel DatabaseManager::getChannelByCustomLink(const std::string& custom_link) {
    Channel channel;
    const char* param_values[1] = {custom_link.c_str()};
    PGresult* res = db_->executePrepared("get_channel_by_custom_link", 1, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        channel.id = std::string(PQgetvalue(res, 0, 0));
        channel.name = std::string(PQgetvalue(res, 0, 1));
        channel.description = PQgetisnull(res, 0, 2) ? "" : std::string(PQgetvalue(res, 0, 2));
        channel.creator_id = std::string(PQgetvalue(res, 0, 3));
        channel.custom_link = std::string(PQgetvalue(res, 0, 4));
        channel.avatar_url = PQgetisnull(res, 0, 5) ? "" : std::string(PQgetvalue(res, 0, 5));
        channel.is_private = PQgetvalue(res, 0, 6)[0] == 't';
        channel.show_author = PQgetvalue(res, 0, 7)[0] == 't';
        channel.created_at = std::string(PQgetvalue(res, 0, 8));
        channel.is_verified = !PQgetisnull(res, 0, 9) && PQgetvalue(res, 0, 9)[0] == 't';
    }
    
    if (res) PQclear(res);
    return channel;
}

std::vector<DatabaseManager::Channel> DatabaseManager::getUserChannels(const std::string& user_id) {
    std::vector<Channel> channels;
    const char* param_values[1] = {user_id.c_str()};
    PGresult* res = db_->executePrepared("get_user_channels", 1, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            Channel channel;
            channel.id = std::string(PQgetvalue(res, i, 0));
            channel.name = std::string(PQgetvalue(res, i, 1));
            channel.description = PQgetisnull(res, i, 2) ? "" : std::string(PQgetvalue(res, i, 2));
            channel.creator_id = std::string(PQgetvalue(res, i, 3));
            channel.custom_link = PQgetisnull(res, i, 4) ? "" : std::string(PQgetvalue(res, i, 4));
            channel.avatar_url = PQgetisnull(res, i, 5) ? "" : std::string(PQgetvalue(res, i, 5));
            channel.is_private = PQgetvalue(res, i, 6)[0] == 't';
            channel.show_author = PQgetvalue(res, i, 7)[0] == 't';
            channel.created_at = std::string(PQgetvalue(res, i, 8));
            channel.is_verified = !PQgetisnull(res, i, 9) && PQgetvalue(res, i, 9)[0] == 't';
            channels.push_back(channel);
        }
    }
    
    if (res) PQclear(res);
    return channels;
}

bool DatabaseManager::addChannelMember(const std::string& channel_id, const std::string& user_id, const std::string& role) {
    const char* param_values[3] = {channel_id.c_str(), user_id.c_str(), role.c_str()};
    PGresult* res = db_->executePrepared("add_channel_member", 3, param_values);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool DatabaseManager::sendChannelMessage(const std::string& channel_id, const std::string& sender_id, const std::string& content,
                                         const std::string& message_type, const std::string& file_path,
                                         const std::string& file_name, long long file_size) {
    const std::string file_size_str = std::to_string(file_size);
    const char* param_values[7] = {
        channel_id.c_str(),
        sender_id.c_str(),
        content.c_str(),
        message_type.c_str(),
        file_path.empty() ? nullptr : file_path.c_str(),
        file_name.empty() ? nullptr : file_name.c_str(),
        file_size_str.c_str()
    };
    
    PGresult* res = db_->executePrepared("send_channel_message", 7, param_values);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

std::vector<DatabaseManager::ChannelMessage> DatabaseManager::getChannelMessages(const std::string& channel_id, int limit) {
    std::vector<ChannelMessage> messages;
    const std::string limit_str = std::to_string(limit);
    const char* param_values[2] = {channel_id.c_str(), limit_str.c_str()};
    PGresult* res = db_->executePrepared("get_channel_messages", 2, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            ChannelMessage msg;
            msg.id = std::string(PQgetvalue(res, i, 0));
            msg.channel_id = std::string(PQgetvalue(res, i, 1));
            msg.sender_id = std::string(PQgetvalue(res, i, 2));
            msg.sender_username = std::string(PQgetvalue(res, i, 3));
            msg.content = std::string(PQgetvalue(res, i, 4));
            msg.message_type = std::string(PQgetvalue(res, i, 5));
            msg.file_path = PQgetisnull(res, i, 6) ? "" : std::string(PQgetvalue(res, i, 6));
            msg.file_name = PQgetisnull(res, i, 7) ? "" : std::string(PQgetvalue(res, i, 7));
            msg.file_size = PQgetisnull(res, i, 8) ? 0 : std::stoll(PQgetvalue(res, i, 8));
            msg.is_pinned = PQgetvalue(res, i, 9)[0] == 't';
            msg.views_count = PQgetisnull(res, i, 10) ? 0 : std::stoi(PQgetvalue(res, i, 10));
            msg.created_at = std::string(PQgetvalue(res, i, 11));
            messages.push_back(msg);
        }
    }
    
    if (res) PQclear(res);
    return messages;
}

bool DatabaseManager::addMessageReaction(const std::string& message_id, const std::string& user_id, const std::string& reaction) {
    const char* param_values[3] = {message_id.c_str(), user_id.c_str(), reaction.c_str()};
    PGresult* res = db_->executePrepared("add_message_reaction", 3, param_values);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool DatabaseManager::removeMessageReaction(const std::string& message_id, const std::string& user_id, const std::string& reaction) {
    const char* param_values[3] = {message_id.c_str(), user_id.c_str(), reaction.c_str()};
    PGresult* res = db_->executePrepared("remove_message_reaction", 3, param_values);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    // Return success only if something was actually deleted (important for toggle semantics)
    bool removed = (PQcmdTuples(res) && std::string(PQcmdTuples(res)) != "0");
    PQclear(res);
    return removed;
}

std::vector<DatabaseManager::MessageReaction> DatabaseManager::getMessageReactions(const std::string& message_id) {
    std::vector<MessageReaction> reactions;
    const char* param_values[1] = {message_id.c_str()};
    PGresult* res = db_->executePrepared("get_message_reactions", 1, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        std::map<std::string, int> reaction_counts;
        for (int i = 0; i < PQntuples(res); i++) {
            std::string reaction = std::string(PQgetvalue(res, i, 0));
            reaction_counts[reaction]++;
        }
        
        for (const auto& pair : reaction_counts) {
            MessageReaction mr;
            mr.reaction = pair.first;
            mr.count = pair.second;
            reactions.push_back(mr);
        }
    }
    
    if (res) PQclear(res);
    return reactions;
}

bool DatabaseManager::addMessageView(const std::string& message_id, const std::string& user_id) {
    const char* param_values[2] = {message_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("add_message_view", 2, param_values);
    
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    
    // Обновляем счетчик просмотров через prepared statement
    const char* update_params[1] = {message_id.c_str()};
    PGresult* update_res = db_->executePrepared("update_channel_message_views", 1, update_params);
    if (update_res) PQclear(update_res);
    
    return true;
}

int DatabaseManager::getMessageViewsCount(const std::string& message_id) {
    const char* param_values[1] = {message_id.c_str()};
    PGresult* res = db_->executePrepared("get_message_views_count", 1, param_values);
    
    int count = 0;
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        count = std::stoi(PQgetvalue(res, 0, 0));
    }
    
    if (res) PQclear(res);
    return count;
}

// Упрощенные реализации остальных методов
bool DatabaseManager::removeChannelMember(const std::string& channel_id, const std::string& user_id) {
    const char* param_values[2] = {channel_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("remove_channel_member", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::updateChannelMemberRole(const std::string& channel_id, const std::string& user_id, const std::string& role) {
    const char* param_values[3] = {role.c_str(), channel_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("update_channel_member_role", 3, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::banChannelMember(const std::string& channel_id, const std::string& user_id, bool banned) {
    const char* param_values[3] = {banned ? "true" : "false", channel_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("ban_channel_member", 3, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

std::vector<DatabaseManager::ChannelMember> DatabaseManager::getChannelMembers(const std::string& channel_id) {
    std::vector<ChannelMember> members;
    const char* param_values[1] = {channel_id.c_str()};
    PGresult* res = db_->executePrepared("get_channel_members", 1, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            ChannelMember member;
            member.id = std::string(PQgetvalue(res, i, 0));
            member.channel_id = std::string(PQgetvalue(res, i, 1));
            member.user_id = std::string(PQgetvalue(res, i, 2));
            member.username = std::string(PQgetvalue(res, i, 3));
            member.role = std::string(PQgetvalue(res, i, 4));
            member.is_banned = PQgetvalue(res, i, 5)[0] == 't';
            member.joined_at = std::string(PQgetvalue(res, i, 6));
            members.push_back(member);
        }
    }
    
    if (res) PQclear(res);
    return members;
}

DatabaseManager::ChannelMember DatabaseManager::getChannelMember(const std::string& channel_id, const std::string& user_id) {
    ChannelMember member;
    const char* param_values[2] = {channel_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("get_channel_member", 2, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        member.id = std::string(PQgetvalue(res, 0, 0));
        member.channel_id = std::string(PQgetvalue(res, 0, 1));
        member.user_id = std::string(PQgetvalue(res, 0, 2));
        member.username = std::string(PQgetvalue(res, 0, 3));
        member.role = std::string(PQgetvalue(res, 0, 4));
        member.is_banned = PQgetvalue(res, 0, 5)[0] == 't';
        member.joined_at = std::string(PQgetvalue(res, 0, 6));
    }
    
    if (res) PQclear(res);
    return member;
}

int DatabaseManager::countChannelSubscribers(const std::string& channel_id) {
    const char* param_values[1] = {channel_id.c_str()};
    PGresult* res = db_->executePrepared("count_channel_subscribers", 1, param_values);
    
    int count = 0;
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        count = std::stoi(std::string(PQgetvalue(res, 0, 0)));
    }
    
    if (res) PQclear(res);
    return count;
}

int DatabaseManager::countChannelMembers(const std::string& channel_id) {
    const char* param_values[1] = {channel_id.c_str()};
    PGresult* res = db_->executePrepared("count_channel_members", 1, param_values);
    
    int count = 0;
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        count = std::stoi(std::string(PQgetvalue(res, 0, 0)));
    }
    
    if (res) PQclear(res);
    return count;
}

bool DatabaseManager::pinChannelMessage(const std::string& channel_id, const std::string& message_id, const std::string& user_id) {
    (void)user_id; // legacy channel_messages has no pinned_by; permission is enforced at handler level
    const char* param_values[2] = {channel_id.c_str(), message_id.c_str()};
    PGresult* res = db_->executePrepared("pin_channel_message", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::unpinChannelMessage(const std::string& channel_id, const std::string& message_id) {
    const char* param_values[2] = {channel_id.c_str(), message_id.c_str()};
    PGresult* res = db_->executePrepared("unpin_channel_message", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::getChannelMessageMetaLegacy(const std::string& message_id, std::string& out_channel_id, std::string& out_sender_id) {
    const char* params[1] = {message_id.c_str()};
    PGresult* res = db_->executePrepared("get_channel_message_meta", 1, params);
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_channel_id = PQgetvalue(res, 0, 0);
    out_sender_id = PQgetvalue(res, 0, 1);
    PQclear(res);
    return true;
}

bool DatabaseManager::deleteChannelMessageLegacy(const std::string& channel_id, const std::string& message_id) {
    const char* params[2] = {message_id.c_str(), channel_id.c_str()};
    PGresult* res = db_->executePrepared("delete_channel_message", 2, params);
    if (!res) return false;
    bool ok = (PQresultStatus(res) == PGRES_COMMAND_OK) && std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::setChannelCustomLink(const std::string& channel_id, const std::string& custom_link) {
    // Если custom_link пустой, устанавливаем NULL
    const char* param_values[2];
    if (custom_link.empty()) {
        param_values[0] = nullptr; // NULL для удаления custom_link
    } else {
        param_values[0] = custom_link.c_str();
    }
    param_values[1] = channel_id.c_str();
    
    PGresult* res = db_->executePrepared("set_channel_custom_link", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::updateChannelName(const std::string& channel_id, const std::string& new_name) {
    const char* param_values[2] = {new_name.c_str(), channel_id.c_str()};
    PGresult* res = db_->executePrepared("update_channel_name", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::updateChannelDescription(const std::string& channel_id, const std::string& new_description) {
    const char* param_values[2] = {new_description.c_str(), channel_id.c_str()};
    PGresult* res = db_->executePrepared("update_channel_description", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::setChannelPrivacy(const std::string& channel_id, bool is_private) {
    const char* param_values[2] = {is_private ? "true" : "false", channel_id.c_str()};
    PGresult* res = db_->executePrepared("set_channel_privacy", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::setChannelShowAuthor(const std::string& channel_id, bool show_author) {
    const char* param_values[2] = {show_author ? "true" : "false", channel_id.c_str()};
    PGresult* res = db_->executePrepared("set_channel_show_author", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::addAllowedReaction(const std::string& channel_id, const std::string& reaction) {
    const char* param_values[2] = {channel_id.c_str(), reaction.c_str()};
    PGresult* res = db_->executePrepared("add_allowed_reaction", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::removeAllowedReaction(const std::string& channel_id, const std::string& reaction) {
    const char* param_values[2] = {channel_id.c_str(), reaction.c_str()};
    PGresult* res = db_->executePrepared("remove_allowed_reaction", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

std::vector<std::string> DatabaseManager::getAllowedReactions(const std::string& channel_id) {
    std::vector<std::string> reactions;
    const char* param_values[1] = {channel_id.c_str()};
    PGresult* res = db_->executePrepared("get_allowed_reactions", 1, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            reactions.push_back(std::string(PQgetvalue(res, i, 0)));
        }
    }
    
    if (res) PQclear(res);
    return reactions;
}

bool DatabaseManager::createChannelJoinRequest(const std::string& channel_id, const std::string& user_id) {
    const char* param_values[2] = {channel_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("create_channel_join_request", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::acceptChannelJoinRequest(const std::string& channel_id, const std::string& user_id) {
    // Удаляем заявку и добавляем участника
    const char* param_values[2] = {channel_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("accept_channel_join_request", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    
    if (success) {
        return addChannelMember(channel_id, user_id, "subscriber");
    }
    return false;
}

bool DatabaseManager::rejectChannelJoinRequest(const std::string& channel_id, const std::string& user_id) {
    const char* param_values[2] = {channel_id.c_str(), user_id.c_str()};
    PGresult* res = db_->executePrepared("reject_channel_join_request", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

std::vector<DatabaseManager::ChannelMember> DatabaseManager::getChannelJoinRequests(const std::string& channel_id) {
    std::vector<ChannelMember> requests;
    const char* param_values[1] = {channel_id.c_str()};
    PGresult* res = db_->executePrepared("get_channel_join_requests", 1, param_values);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        for (int i = 0; i < PQntuples(res); i++) {
            ChannelMember member;
            member.id = std::string(PQgetvalue(res, i, 0));
            member.channel_id = std::string(PQgetvalue(res, i, 1));
            member.user_id = std::string(PQgetvalue(res, i, 2));
            member.username = std::string(PQgetvalue(res, i, 3));
            member.joined_at = std::string(PQgetvalue(res, i, 4));
            requests.push_back(member);
        }
    }
    
    if (res) PQclear(res);
    return requests;
}

bool DatabaseManager::createChannelChat(const std::string& channel_id, std::string& group_id) {
    // Получаем информацию о канале для создания группы
    Channel channel = getChannelById(channel_id);
    if (channel.id.empty()) {
        return false;
    }
    
    // Создаем группу для чата канала
    std::string chat_name = "Chat: " + channel.name;
    std::string chat_description = "Chat for channel " + channel.name;
    if (!createGroup(chat_name, chat_description, channel.creator_id, group_id)) {
        return false;
    }
    
    // Связываем группу с каналом
    const char* param_values[2] = {channel_id.c_str(), group_id.c_str()};
    PGresult* res = db_->executePrepared("create_channel_chat", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::linkChannelChat(const std::string& channel_id, const std::string& group_id) {
    const char* param_values[2] = {channel_id.c_str(), group_id.c_str()};
    PGresult* res = db_->executePrepared("create_channel_chat", 2, param_values);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::updateChannelAvatar(const std::string& channel_id, const std::string& avatar_url) {
    const char* params[2] = {avatar_url.c_str(), channel_id.c_str()};
    PGresult* res = db_->executePrepared("update_channel_avatar", 2, params);
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    return success;
}

bool DatabaseManager::deleteChannelLegacy(const std::string& channel_id) {
    const char* params[1] = {channel_id.c_str()};
    PGresult* res = db_->executePrepared("delete_channel_legacy", 1, params);
    if (!res) return false;
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

// ============================================
// Public Directory Methods
// ============================================

std::vector<DatabaseManager::Channel> DatabaseManager::getPublicChannels(const std::string& category, const std::string& search, int limit) {
    std::vector<Channel> channels;
    
    // Build query dynamically based on parameters
    // is_private = false means it's public
    std::string query = "SELECT c.id, c.name, c.description, c.creator_id, COALESCE(c.custom_link, ''), c.created_at, "
                        "COALESCE(c.category, '') as category, COALESCE(c.is_verified, false) as is_verified, "
                        "COALESCE(c.avatar_url, '') as avatar_url "
                        "FROM channels c WHERE COALESCE(c.is_private, false) = false";
    
    std::vector<std::string> params_vec;
    int param_num = 1;
    
    if (!category.empty() && category != "all" && category != "trending") {
        query += " AND LOWER(COALESCE(c.category, '')) = LOWER($" + std::to_string(param_num++) + ")";
        params_vec.push_back(category);
    }
    
    if (!search.empty()) {
        query += " AND (LOWER(c.name) LIKE LOWER($" + std::to_string(param_num) + ") OR LOWER(COALESCE(c.description, '')) LIKE LOWER($" + std::to_string(param_num) + "))";
        params_vec.push_back("%" + search + "%");
        param_num++;
    }
    
    query += " ORDER BY c.created_at DESC LIMIT " + std::to_string(limit);
    
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
            Channel channel;
            channel.id = PQgetisnull(res, i, 0) ? "" : std::string(PQgetvalue(res, i, 0));
            channel.name = PQgetisnull(res, i, 1) ? "" : std::string(PQgetvalue(res, i, 1));
            channel.description = PQgetisnull(res, i, 2) ? "" : std::string(PQgetvalue(res, i, 2));
            channel.creator_id = PQgetisnull(res, i, 3) ? "" : std::string(PQgetvalue(res, i, 3));
            channel.custom_link = PQgetisnull(res, i, 4) ? "" : std::string(PQgetvalue(res, i, 4));
            channel.created_at = PQgetisnull(res, i, 5) ? "" : std::string(PQgetvalue(res, i, 5));
            channel.category = PQgetisnull(res, i, 6) ? "" : std::string(PQgetvalue(res, i, 6));
            channel.is_verified = !PQgetisnull(res, i, 7) && std::string(PQgetvalue(res, i, 7)) == "t";
            channel.avatar_url = PQgetisnull(res, i, 8) ? "" : std::string(PQgetvalue(res, i, 8));
            channels.push_back(channel);
        }
    }
    
    if (res) PQclear(res);
    return channels;
}

int DatabaseManager::getChannelSubscribersCount(const std::string& channel_id) {
    const char* params[1] = {channel_id.c_str()};
    // channel_members is the table for subscribers (not channel_subscribers)
    std::string query = "SELECT COUNT(*) FROM channel_members WHERE channel_id = $1 AND is_banned = false";
    
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

bool DatabaseManager::isChannelSubscriber(const std::string& channel_id, const std::string& user_id) {
    const char* params[2] = {channel_id.c_str(), user_id.c_str()};
    // channel_members is the table for subscribers (not channel_subscribers)
    std::string query = "SELECT 1 FROM channel_members WHERE channel_id = $1 AND user_id = $2 AND is_banned = false LIMIT 1";
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 2, nullptr, params, nullptr, nullptr, 0);
    
    bool is_subscriber = (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0);
    
    if (res) PQclear(res);
    return is_subscriber;
}

bool DatabaseManager::setChannelPublicDirectory(const std::string& channel_id, bool is_public, const std::string& category) {
    // is_public = true means is_private = false
    std::string is_private_str = is_public ? "FALSE" : "TRUE";
    std::string query = "UPDATE channels SET is_private = " + is_private_str + ", category = $1 WHERE id = $2";
    
    const char* params[2] = {category.c_str(), channel_id.c_str()};
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 2, nullptr, params, nullptr, nullptr, 0);
    
    bool success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
    if (res) PQclear(res);
    
    return success;
}

} // namespace xipher
