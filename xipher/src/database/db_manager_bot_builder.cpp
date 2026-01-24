// Bot Builder database operations
// OWASP 2025 Security Compliant

#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>
#include <cstring>
#include <libpq-fe.h>

namespace xipher {

bool DatabaseManager::createBotBuilderBot(const std::string& user_id, const std::string& bot_user_id, const std::string& bot_token,
                                          const std::string& bot_username, const std::string& bot_name,
                                          const std::string& flow_json, std::string& bot_id) {
    const char* param_values[6] = {
        user_id.c_str(),
        bot_user_id.c_str(),
        bot_token.c_str(),
        bot_username.c_str(),
        bot_name.c_str(),
        flow_json.c_str()
    };
    
    PGresult* res = db_->executePrepared("create_bot_builder_bot", 6, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        Logger::getInstance().error("Failed to create bot: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    if (PQntuples(res) > 0) {
        bot_id = std::string(PQgetvalue(res, 0, 0));
    }
    
    PQclear(res);
    return !bot_id.empty();
}

bool DatabaseManager::updateBotBuilderBot(const std::string& bot_id, const std::string& flow_json, bool is_active) {
    std::string active_str = is_active ? "TRUE" : "FALSE";
    
    const char* param_values[3] = {
        flow_json.c_str(),
        active_str.c_str(),
        bot_id.c_str()
    };
    
    PGresult* res = db_->executePrepared("update_bot_builder_bot", 3, param_values);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("Failed to update bot: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool DatabaseManager::deleteBotBuilderBot(const std::string& bot_id) {
    const char* param_values[1] = {bot_id.c_str()};
    
    PGresult* res = db_->executePrepared("delete_bot_builder_bot", 1, param_values);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("Failed to delete bot: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

std::vector<DatabaseManager::BotBuilderBot> DatabaseManager::getUserBots(const std::string& user_id) {
    std::vector<BotBuilderBot> bots;
    
    const char* param_values[1] = {user_id.c_str()};
    
    PGresult* res = db_->executePrepared("get_user_bots", 1, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        Logger::getInstance().error("Failed to get user bots: " + db_->getLastError());
        if (res) PQclear(res);
        return bots;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        BotBuilderBot bot;
        bot.id = std::string(PQgetvalue(res, i, 0));
        bot.user_id = std::string(PQgetvalue(res, i, 1));
        bot.bot_user_id = PQgetvalue(res, i, 2) ? std::string(PQgetvalue(res, i, 2)) : "";
        bot.bot_token = std::string(PQgetvalue(res, i, 3));
        bot.bot_username = PQgetvalue(res, i, 4) ? std::string(PQgetvalue(res, i, 4)) : "";
        bot.bot_name = std::string(PQgetvalue(res, i, 5));
        bot.flow_json = PQgetvalue(res, i, 6) ? std::string(PQgetvalue(res, i, 6)) : "{}";
        bot.is_active = std::string(PQgetvalue(res, i, 7)) == "t";
        bot.created_at = std::string(PQgetvalue(res, i, 8));
        bot.deployed_at = PQgetvalue(res, i, 9) ? std::string(PQgetvalue(res, i, 9)) : "";
        bot.bot_description = PQgetvalue(res, i, 10) ? std::string(PQgetvalue(res, i, 10)) : "";
        bot.bot_avatar_url = PQgetvalue(res, i, 11) ? std::string(PQgetvalue(res, i, 11)) : "";
        bots.push_back(bot);
    }
    
    PQclear(res);
    return bots;
}

DatabaseManager::BotBuilderBot DatabaseManager::getBotBuilderBotByToken(const std::string& bot_token) {
    BotBuilderBot bot;
    
    const char* param_values[1] = {bot_token.c_str()};
    
    PGresult* res = db_->executePrepared("get_bot_builder_bot_by_token", 1, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        Logger::getInstance().error("Failed to get bot by token: " + db_->getLastError());
        if (res) PQclear(res);
        return bot;
    }
    
    bot.id = std::string(PQgetvalue(res, 0, 0));
    bot.user_id = std::string(PQgetvalue(res, 0, 1));
    bot.bot_user_id = PQgetvalue(res, 0, 2) ? std::string(PQgetvalue(res, 0, 2)) : "";
    bot.bot_token = std::string(PQgetvalue(res, 0, 3));
    bot.bot_username = PQgetvalue(res, 0, 4) ? std::string(PQgetvalue(res, 0, 4)) : "";
    bot.bot_name = std::string(PQgetvalue(res, 0, 5));
    bot.flow_json = PQgetvalue(res, 0, 6) ? std::string(PQgetvalue(res, 0, 6)) : "{}";
    bot.is_active = std::string(PQgetvalue(res, 0, 7)) == "t";
    bot.created_at = std::string(PQgetvalue(res, 0, 8));
    bot.deployed_at = PQgetvalue(res, 0, 9) ? std::string(PQgetvalue(res, 0, 9)) : "";
    bot.bot_description = PQgetvalue(res, 0, 10) ? std::string(PQgetvalue(res, 0, 10)) : "";
    bot.bot_avatar_url = PQgetvalue(res, 0, 11) ? std::string(PQgetvalue(res, 0, 11)) : "";
    
    PQclear(res);
    return bot;
}

DatabaseManager::BotBuilderBot DatabaseManager::getBotBuilderBotByUsername(const std::string& bot_username) {
    BotBuilderBot bot;

    const char* param_values[1] = {bot_username.c_str()};

    PGresult* res = db_->executePrepared("get_bot_builder_bot_by_username", 1, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return bot;
    }

    bot.id = std::string(PQgetvalue(res, 0, 0));
    bot.user_id = std::string(PQgetvalue(res, 0, 1));
    bot.bot_user_id = PQgetvalue(res, 0, 2) ? std::string(PQgetvalue(res, 0, 2)) : "";
    bot.bot_token = std::string(PQgetvalue(res, 0, 3));
    bot.bot_username = PQgetvalue(res, 0, 4) ? std::string(PQgetvalue(res, 0, 4)) : "";
    bot.bot_name = std::string(PQgetvalue(res, 0, 5));
    bot.flow_json = PQgetvalue(res, 0, 6) ? std::string(PQgetvalue(res, 0, 6)) : "{}";
    bot.is_active = std::string(PQgetvalue(res, 0, 7)) == "t";
    bot.created_at = std::string(PQgetvalue(res, 0, 8));
    bot.deployed_at = PQgetvalue(res, 0, 9) ? std::string(PQgetvalue(res, 0, 9)) : "";
    bot.bot_description = PQgetvalue(res, 0, 10) ? std::string(PQgetvalue(res, 0, 10)) : "";
    bot.bot_avatar_url = PQgetvalue(res, 0, 11) ? std::string(PQgetvalue(res, 0, 11)) : "";

    PQclear(res);
    return bot;
}

DatabaseManager::BotBuilderBot DatabaseManager::getBotBuilderBotById(const std::string& bot_id) {
    BotBuilderBot bot;

    const char* param_values[1] = {bot_id.c_str()};

    PGresult* res = db_->executePrepared("get_bot_builder_bot_by_id", 1, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return bot;
    }

    bot.id = std::string(PQgetvalue(res, 0, 0));
    bot.user_id = std::string(PQgetvalue(res, 0, 1));
    bot.bot_user_id = PQgetvalue(res, 0, 2) ? std::string(PQgetvalue(res, 0, 2)) : "";
    bot.bot_token = std::string(PQgetvalue(res, 0, 3));
    bot.bot_username = PQgetvalue(res, 0, 4) ? std::string(PQgetvalue(res, 0, 4)) : "";
    bot.bot_name = std::string(PQgetvalue(res, 0, 5));
    bot.flow_json = PQgetvalue(res, 0, 6) ? std::string(PQgetvalue(res, 0, 6)) : "{}";
    bot.is_active = std::string(PQgetvalue(res, 0, 7)) == "t";
    bot.created_at = std::string(PQgetvalue(res, 0, 8));
    bot.deployed_at = PQgetvalue(res, 0, 9) ? std::string(PQgetvalue(res, 0, 9)) : "";
    bot.bot_description = PQgetvalue(res, 0, 10) ? std::string(PQgetvalue(res, 0, 10)) : "";
    bot.bot_avatar_url = PQgetvalue(res, 0, 11) ? std::string(PQgetvalue(res, 0, 11)) : "";

    PQclear(res);
    return bot;
}

DatabaseManager::BotBuilderBot DatabaseManager::getBotBuilderBotByUserId(const std::string& bot_user_id) {
    BotBuilderBot bot;

    const char* param_values[1] = {bot_user_id.c_str()};

    PGresult* res = db_->executePrepared("get_bot_builder_bot_by_user_id", 1, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return bot;
    }

    bot.id = std::string(PQgetvalue(res, 0, 0));
    bot.user_id = std::string(PQgetvalue(res, 0, 1));
    bot.bot_user_id = PQgetvalue(res, 0, 2) ? std::string(PQgetvalue(res, 0, 2)) : "";
    bot.bot_token = std::string(PQgetvalue(res, 0, 3));
    bot.bot_username = PQgetvalue(res, 0, 4) ? std::string(PQgetvalue(res, 0, 4)) : "";
    bot.bot_name = std::string(PQgetvalue(res, 0, 5));
    bot.flow_json = PQgetvalue(res, 0, 6) ? std::string(PQgetvalue(res, 0, 6)) : "{}";
    bot.is_active = std::string(PQgetvalue(res, 0, 7)) == "t";
    bot.created_at = std::string(PQgetvalue(res, 0, 8));
    bot.deployed_at = PQgetvalue(res, 0, 9) ? std::string(PQgetvalue(res, 0, 9)) : "";
    bot.bot_description = PQgetvalue(res, 0, 10) ? std::string(PQgetvalue(res, 0, 10)) : "";
    bot.bot_avatar_url = PQgetvalue(res, 0, 11) ? std::string(PQgetvalue(res, 0, 11)) : "";

    PQclear(res);
    return bot;
}

bool DatabaseManager::updateBotBuilderProfile(const std::string& bot_id, const std::string& bot_name, const std::string& bot_description) {
    const char* param_values[3] = {bot_name.c_str(), bot_description.c_str(), bot_id.c_str()};
    PGresult* res = db_->executePrepared("update_bot_builder_profile", 3, param_values);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::updateBotBuilderAvatarUrl(const std::string& bot_id, const std::string& bot_avatar_url) {
    const char* param_values[2] = {bot_avatar_url.c_str(), bot_id.c_str()};
    PGresult* res = db_->executePrepared("update_bot_builder_avatar", 2, param_values);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    bool ok = std::string(PQcmdTuples(res)) != "0";
    PQclear(res);
    return ok;
}

bool DatabaseManager::logBotExecution(const std::string& bot_id, const std::string& update_id,
                                      const std::string& user_id, const std::string& chat_id,
                                      int node_id, const std::string& node_type,
                                      const std::string& execution_result, int execution_time_ms,
                                      const std::string& error_message) {
    std::string node_id_str = std::to_string(node_id);
    std::string time_ms_str = std::to_string(execution_time_ms);
    std::string chat_id_val = chat_id.empty() ? "NULL" : chat_id;
    std::string user_id_val = user_id.empty() ? "NULL" : user_id;
    std::string error_val = error_message.empty() ? "NULL" : error_message;
    
    const char* param_values[9] = {
        bot_id.c_str(),
        update_id.c_str(),
        user_id_val == "NULL" ? nullptr : user_id_val.c_str(),
        chat_id_val == "NULL" ? nullptr : chat_id_val.c_str(),
        node_id_str.c_str(),
        node_type.c_str(),
        execution_result.c_str(),
        time_ms_str.c_str(),
        error_val == "NULL" ? nullptr : error_val.c_str()
    };
    
    PGresult* res = db_->executePrepared("log_bot_execution", 9, param_values);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("Failed to log bot execution: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

}

