#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>

namespace xipher {

bool DatabaseManager::upsertBotFile(const std::string& bot_id, const std::string& file_path, const std::string& file_content) {
    const char* param_values[3] = {
        bot_id.c_str(),
        file_path.c_str(),
        file_content.c_str()
    };
    PGresult* res = db_->executePrepared("upsert_bot_file", 3, param_values);
    if (!res) {
        Logger::getInstance().error("Failed to upsert bot file: " + db_->getLastError());
        return false;
    }
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

bool DatabaseManager::getBotFile(const std::string& bot_id, const std::string& file_path, std::string& out_content) {
    const char* param_values[2] = {
        bot_id.c_str(),
        file_path.c_str()
    };
    PGresult* res = db_->executePrepared("get_bot_file", 2, param_values);
    if (!res) {
        Logger::getInstance().error("Failed to get bot file: " + db_->getLastError());
        return false;
    }
    if (PQntuples(res) == 0) {
        PQclear(res);
        return false;
    }
    out_content = PQgetvalue(res, 0, 0) ? std::string(PQgetvalue(res, 0, 0)) : "";
    PQclear(res);
    return true;
}

bool DatabaseManager::deleteBotFile(const std::string& bot_id, const std::string& file_path) {
    const char* param_values[2] = {
        bot_id.c_str(),
        file_path.c_str()
    };
    PGresult* res = db_->executePrepared("delete_bot_file", 2, param_values);
    if (!res) {
        Logger::getInstance().error("Failed to delete bot file: " + db_->getLastError());
        return false;
    }
    bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
    PQclear(res);
    return success;
}

std::vector<DatabaseManager::BotFile> DatabaseManager::listBotFiles(const std::string& bot_id) {
    std::vector<BotFile> out;
    const char* param_values[1] = { bot_id.c_str() };
    PGresult* res = db_->executePrepared("list_bot_files", 1, param_values);
    if (!res) {
        Logger::getInstance().error("Failed to list bot files: " + db_->getLastError());
        return out;
    }
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        BotFile f;
        f.id = PQgetvalue(res, i, 0) ? std::string(PQgetvalue(res, i, 0)) : "";
        f.bot_id = bot_id;
        f.file_path = PQgetvalue(res, i, 1) ? std::string(PQgetvalue(res, i, 1)) : "";
        f.file_content = PQgetvalue(res, i, 2) ? std::string(PQgetvalue(res, i, 2)) : "";
        f.created_at = PQgetvalue(res, i, 3) ? std::string(PQgetvalue(res, i, 3)) : "";
        f.updated_at = PQgetvalue(res, i, 4) ? std::string(PQgetvalue(res, i, 4)) : "";
        out.push_back(f);
    }
    PQclear(res);
    return out;
}

} // namespace xipher

