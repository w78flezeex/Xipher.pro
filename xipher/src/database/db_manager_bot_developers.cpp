#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>

namespace xipher {

bool DatabaseManager::addBotDeveloper(const std::string& bot_id, const std::string& developer_user_id, const std::string& added_by_user_id) {
    const char* param_values[3] = {
        bot_id.c_str(),
        developer_user_id.c_str(),
        added_by_user_id.c_str()
    };
    PGresult* res = db_->executePrepared("add_bot_developer", 3, param_values);
    if (!res) {
        Logger::getInstance().error("Failed to add bot developer: " + db_->getLastError());
        return false;
    }
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

bool DatabaseManager::removeBotDeveloper(const std::string& bot_id, const std::string& developer_user_id) {
    const char* param_values[2] = {
        bot_id.c_str(),
        developer_user_id.c_str()
    };
    PGresult* res = db_->executePrepared("remove_bot_developer", 2, param_values);
    if (!res) {
        Logger::getInstance().error("Failed to remove bot developer: " + db_->getLastError());
        return false;
    }
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

std::vector<DatabaseManager::BotDeveloper> DatabaseManager::getBotDevelopers(const std::string& bot_id) {
    std::vector<BotDeveloper> out;
    const char* param_values[1] = { bot_id.c_str() };
    PGresult* res = db_->executePrepared("get_bot_developers", 1, param_values);
    if (!res) {
        Logger::getInstance().error("Failed to get bot developers: " + db_->getLastError());
        return out;
    }
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        BotDeveloper dev;
        dev.developer_user_id = PQgetvalue(res, i, 0) ? std::string(PQgetvalue(res, i, 0)) : "";
        dev.developer_username = PQgetvalue(res, i, 1) ? std::string(PQgetvalue(res, i, 1)) : "";
        dev.added_by_user_id = PQgetvalue(res, i, 2) ? std::string(PQgetvalue(res, i, 2)) : "";
        dev.added_at = PQgetvalue(res, i, 3) ? std::string(PQgetvalue(res, i, 3)) : "";
        out.push_back(dev);
    }
    PQclear(res);
    return out;
}

bool DatabaseManager::isBotDeveloper(const std::string& bot_id, const std::string& user_id) {
    const char* param_values[2] = { bot_id.c_str(), user_id.c_str() };
    PGresult* res = db_->executePrepared("is_bot_developer", 2, param_values);
    if (!res) {
        return false;
    }
    bool found = (PQntuples(res) > 0);
    PQclear(res);
    return found;
}

bool DatabaseManager::canEditBot(const std::string& bot_id, const std::string& user_id) {
    auto bot = getBotBuilderBotById(bot_id);
    if (bot.id.empty()) return false;
    // Owner can always edit
    if (bot.user_id == user_id) return true;
    // Check if user is a developer
    return isBotDeveloper(bot_id, user_id);
}

} // namespace xipher

