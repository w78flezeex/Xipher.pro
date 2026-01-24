// Event Router database operations
// OWASP 2025 Security Compliant

#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>
#include <cstring>
#include <optional>
#include <libpq-fe.h>

namespace xipher {

bool DatabaseManager::createTriggerRule(const std::string& chat_id, const std::string& chat_type,
                                       const std::string& rule_name, const std::string& trigger_conditions,
                                       const std::string& actions, int rate_limit_per_second,
                                       const std::string& created_by, std::string& rule_id) {
    std::string rate_limit_str = std::to_string(rate_limit_per_second);
    
    const char* param_values[7] = {
        chat_id.c_str(),
        chat_type.c_str(),
        rule_name.c_str(),
        trigger_conditions.c_str(),
        actions.c_str(),
        rate_limit_str.c_str(),
        created_by.c_str()
    };
    
    PGresult* res = db_->executePrepared("create_trigger_rule", 7, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        Logger::getInstance().error("Failed to create trigger rule: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    if (PQntuples(res) > 0) {
        rule_id = std::string(PQgetvalue(res, 0, 0));
    }
    
    PQclear(res);
    return !rule_id.empty();
}

bool DatabaseManager::updateTriggerRule(const std::string& rule_id, const std::string& trigger_conditions,
                                        const std::string& actions, bool is_active, int rate_limit_per_second) {
    std::string active_str = is_active ? "TRUE" : "FALSE";
    std::string rate_limit_str = std::to_string(rate_limit_per_second);
    
    const char* param_values[5] = {
        trigger_conditions.c_str(),
        actions.c_str(),
        active_str.c_str(),
        rate_limit_str.c_str(),
        rule_id.c_str()
    };
    
    PGresult* res = db_->executePrepared("update_trigger_rule", 5, param_values);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("Failed to update trigger rule: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool DatabaseManager::deleteTriggerRule(const std::string& rule_id) {
    const char* param_values[1] = {rule_id.c_str()};
    
    PGresult* res = db_->executePrepared("delete_trigger_rule", 1, param_values);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("Failed to delete trigger rule: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

std::vector<DatabaseManager::TriggerRule> DatabaseManager::getTriggerRules(
    const std::string& chat_id, const std::string& chat_type) {
    std::vector<TriggerRule> rules;
    
    const char* param_values[2] = {chat_id.c_str(), chat_type.c_str()};
    
    PGresult* res = db_->executePrepared("get_trigger_rules", 2, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        Logger::getInstance().error("Failed to get trigger rules: " + db_->getLastError());
        if (res) PQclear(res);
        return rules;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        TriggerRule rule;
        rule.id = std::string(PQgetvalue(res, i, 0));
        rule.chat_id = std::string(PQgetvalue(res, i, 1));
        rule.chat_type = std::string(PQgetvalue(res, i, 2));
        rule.rule_name = std::string(PQgetvalue(res, i, 3));
        rule.trigger_conditions = PQgetvalue(res, i, 4) ? std::string(PQgetvalue(res, i, 4)) : "{}";
        rule.actions = PQgetvalue(res, i, 5) ? std::string(PQgetvalue(res, i, 5)) : "{}";
        rule.is_active = std::string(PQgetvalue(res, i, 6)) == "t";
        rule.rate_limit_per_second = std::stoi(PQgetvalue(res, i, 7));
        rule.created_at = std::string(PQgetvalue(res, i, 8));
        rule.created_by = std::string(PQgetvalue(res, i, 9));
        rules.push_back(rule);
    }
    
    PQclear(res);
    return rules;
}

std::optional<DatabaseManager::TriggerRule> DatabaseManager::getTriggerRuleById(const std::string& rule_id) {
    // Direct SQL query to get trigger rule by ID
    // OWASP 2025: A04 - BOLA protection - need to get rule details for ownership verification
    std::string query = "SELECT id, chat_id, chat_type, rule_name, trigger_conditions, actions, "
                        "is_active, rate_limit_per_second, created_at, created_by "
                        "FROM trigger_rules WHERE id = $1";
    
    const char* param_values[1] = {rule_id.c_str()};
    
    PGresult* res = PQexecParams(db_->getConnection(), query.c_str(), 1, nullptr, 
                                  param_values, nullptr, nullptr, 0);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        Logger::getInstance().error("Failed to get trigger rule by id: " + std::string(PQerrorMessage(db_->getConnection())));
        if (res) PQclear(res);
        return std::nullopt;
    }
    
    if (PQntuples(res) == 0) {
        PQclear(res);
        return std::nullopt;
    }
    
    TriggerRule rule;
    rule.id = std::string(PQgetvalue(res, 0, 0));
    rule.chat_id = std::string(PQgetvalue(res, 0, 1));
    rule.chat_type = std::string(PQgetvalue(res, 0, 2));
    rule.rule_name = std::string(PQgetvalue(res, 0, 3));
    rule.trigger_conditions = PQgetvalue(res, 0, 4) ? std::string(PQgetvalue(res, 0, 4)) : "{}";
    rule.actions = PQgetvalue(res, 0, 5) ? std::string(PQgetvalue(res, 0, 5)) : "{}";
    rule.is_active = std::string(PQgetvalue(res, 0, 6)) == "t";
    rule.rate_limit_per_second = std::stoi(PQgetvalue(res, 0, 7));
    rule.created_at = std::string(PQgetvalue(res, 0, 8));
    rule.created_by = std::string(PQgetvalue(res, 0, 9));
    
    PQclear(res);
    return rule;
}

bool DatabaseManager::createEventWebhook(const std::string& trigger_rule_id, const std::string& webhook_url,
                                         const std::string& secret_token, const std::string& http_method,
                                         const std::string& headers_json, std::string& webhook_id) {
    const char* param_values[5] = {
        trigger_rule_id.c_str(),
        webhook_url.c_str(),
        secret_token.c_str(),
        http_method.c_str(),
        headers_json.c_str()
    };
    
    PGresult* res = db_->executePrepared("create_event_webhook", 5, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        Logger::getInstance().error("Failed to create event webhook: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    if (PQntuples(res) > 0) {
        webhook_id = std::string(PQgetvalue(res, 0, 0));
    }
    
    PQclear(res);
    return !webhook_id.empty();
}

bool DatabaseManager::logTriggerExecution(const std::string& trigger_rule_id, const std::string& event_type,
                                          const std::string& event_data, const std::string& execution_result,
                                          const std::string& status, const std::string& error_message) {
    std::string error_val = error_message.empty() ? "NULL" : error_message;
    
    const char* param_values[6] = {
        trigger_rule_id.c_str(),
        event_type.c_str(),
        event_data.c_str(),
        execution_result.c_str(),
        status.c_str(),
        error_val == "NULL" ? nullptr : error_val.c_str()
    };
    
    PGresult* res = db_->executePrepared("log_trigger_execution", 6, param_values);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("Failed to log trigger execution: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

}

