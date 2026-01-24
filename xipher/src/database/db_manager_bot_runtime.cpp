// Bot runtime database operations (notes + reminders + group bot lookup)

#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"

#include <libpq-fe.h>

namespace xipher {

bool DatabaseManager::upsertBotNote(const std::string& bot_user_id,
                                   const std::string& scope_type,
                                   const std::string& scope_id,
                                   const std::string& note_key,
                                   const std::string& note_value,
                                   const std::string& created_by_user_id) {
    const char* params[6] = {
        bot_user_id.c_str(),
        scope_type.c_str(),
        scope_id.c_str(),
        note_key.c_str(),
        note_value.c_str(),
        created_by_user_id.c_str()
    };
    PGresult* res = db_->executePrepared("upsert_bot_note", 6, params);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}

bool DatabaseManager::getBotNote(const std::string& bot_user_id,
                                const std::string& scope_type,
                                const std::string& scope_id,
                                const std::string& note_key,
                                std::string& out_note_value) {
    const char* params[4] = {
        bot_user_id.c_str(),
        scope_type.c_str(),
        scope_id.c_str(),
        note_key.c_str()
    };
    PGresult* res = db_->executePrepared("get_bot_note", 4, params);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_note_value = PQgetvalue(res, 0, 0);
    PQclear(res);
    return true;
}

bool DatabaseManager::listBotNotes(const std::string& bot_user_id,
                                  const std::string& scope_type,
                                  const std::string& scope_id,
                                  std::vector<std::string>& out_keys) {
    out_keys.clear();
    const char* params[3] = {
        bot_user_id.c_str(),
        scope_type.c_str(),
        scope_id.c_str()
    };
    PGresult* res = db_->executePrepared("list_bot_notes", 3, params);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        if (res) PQclear(res);
        return false;
    }
    const int rows = PQntuples(res);
    out_keys.reserve(static_cast<size_t>(rows));
    for (int i = 0; i < rows; i++) {
        out_keys.push_back(PQgetvalue(res, i, 0));
    }
    PQclear(res);
    return true;
}

bool DatabaseManager::deleteBotNote(const std::string& bot_user_id,
                                   const std::string& scope_type,
                                   const std::string& scope_id,
                                   const std::string& note_key) {
    const char* params[4] = {
        bot_user_id.c_str(),
        scope_type.c_str(),
        scope_id.c_str(),
        note_key.c_str()
    };
    PGresult* res = db_->executePrepared("delete_bot_note", 4, params);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        if (res) PQclear(res);
        return false;
    }
    PQclear(res);
    return true;
}

bool DatabaseManager::createBotReminder(const std::string& bot_user_id,
                                       const std::string& scope_type,
                                       const std::string& scope_id,
                                       const std::string& target_user_id,
                                       const std::string& reminder_text,
                                       int delay_seconds,
                                       std::string& out_reminder_id) {
    out_reminder_id.clear();
    const std::string delay = std::to_string(delay_seconds);
    const char* params[6] = {
        bot_user_id.c_str(),
        scope_type.c_str(),
        scope_id.c_str(),
        target_user_id.c_str(),
        reminder_text.c_str(),
        delay.c_str()
    };
    PGresult* res = db_->executePrepared("create_bot_reminder", 6, params);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        if (res) PQclear(res);
        return false;
    }
    out_reminder_id = PQgetvalue(res, 0, 0);
    PQclear(res);
    return !out_reminder_id.empty();
}

std::vector<DatabaseManager::BotReminderDue> DatabaseManager::claimDueBotReminders(int limit) {
    std::vector<BotReminderDue> out;
    const std::string lim = std::to_string(limit);
    const char* params[1] = {lim.c_str()};
    PGresult* res = db_->executePrepared("claim_due_bot_reminders", 1, params);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        if (res) PQclear(res);
        return out;
    }
    const int rows = PQntuples(res);
    out.reserve(static_cast<size_t>(rows));
    for (int i = 0; i < rows; i++) {
        BotReminderDue r;
        r.id = PQgetvalue(res, i, 0);
        r.bot_user_id = PQgetvalue(res, i, 1);
        r.scope_type = PQgetvalue(res, i, 2);
        r.scope_id = PQgetvalue(res, i, 3);
        r.target_user_id = PQgetvalue(res, i, 4);
        r.text = PQgetvalue(res, i, 5);
        out.push_back(std::move(r));
    }
    PQclear(res);
    return out;
}

std::vector<std::string> DatabaseManager::getGroupBotUserIds(const std::string& group_id) {
    std::vector<std::string> out;
    const char* params[1] = {group_id.c_str()};
    PGresult* res = db_->executePrepared("get_group_bot_user_ids", 1, params);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        if (res) PQclear(res);
        return out;
    }
    const int rows = PQntuples(res);
    out.reserve(static_cast<size_t>(rows));
    for (int i = 0; i < rows; i++) {
        out.push_back(PQgetvalue(res, i, 0));
    }
    PQclear(res);
    return out;
}

} // namespace xipher


