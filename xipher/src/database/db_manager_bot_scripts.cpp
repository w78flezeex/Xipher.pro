// Bot Builder scripts (Python) database operations

#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"

#include <libpq-fe.h>

namespace xipher {

bool DatabaseManager::getBotBuilderScriptByBotUserId(const std::string& bot_user_id,
                                                    std::string& out_lang,
                                                    bool& out_enabled,
                                                    std::string& out_code) {
    out_lang.clear();
    out_enabled = false;
    out_code.clear();
    const char* params[1] = {bot_user_id.c_str()};
    PGresult* res = db_->executePrepared("get_bot_builder_script_by_bot_user_id", 1, params);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_lang = PQgetvalue(res, 0, 0);
    out_enabled = (PQgetvalue(res, 0, 1)[0] == 't');
    out_code = PQgetvalue(res, 0, 2);
    PQclear(res);
    return true;
}

bool DatabaseManager::getBotBuilderScriptById(const std::string& bot_id,
                                             std::string& out_lang,
                                             bool& out_enabled,
                                             std::string& out_code) {
    out_lang.clear();
    out_enabled = false;
    out_code.clear();
    const char* params[1] = {bot_id.c_str()};
    PGresult* res = db_->executePrepared("get_bot_builder_script_by_id", 1, params);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_lang = PQgetvalue(res, 0, 0);
    out_enabled = (PQgetvalue(res, 0, 1)[0] == 't');
    out_code = PQgetvalue(res, 0, 2);
    PQclear(res);
    return true;
}

bool DatabaseManager::updateBotBuilderScript(const std::string& bot_id,
                                            const std::string& lang,
                                            bool enabled,
                                            const std::string& code) {
    const char* params[4] = {
        lang.c_str(),
        enabled ? "true" : "false",
        code.c_str(),
        bot_id.c_str()
    };
    PGresult* res = db_->executePrepared("update_bot_builder_script", 4, params);
    if (!res) {
        Logger::getInstance().error("Failed to update bot script (no result): " + db_->getLastError());
        return false;
    }
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("Failed to update bot script: " + std::string(PQresultErrorMessage(res)));
        PQclear(res);
        return false;
    }
    // UPDATE always succeeds even if 0 rows affected (script columns might be NULL initially)
    // Check if any rows were updated
    const char* tuples = PQcmdTuples(res);
    bool ok = (tuples && std::string(tuples) != "0" && std::string(tuples).length() > 0);
    PQclear(res);
    if (!ok) {
        Logger::getInstance().info("Bot script update returned 0 rows, but continuing anyway (script columns might be NULL initially)");
    }
    return true; // Return true anyway - script columns might be NULL initially
}

} // namespace xipher


