// Event Router API handlers
// OWASP 2025 Security Compliant

#include "../include/server/request_handler.hpp"
#include "../include/utils/json_parser.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>

namespace xipher {

std::string RequestHandler::handleCreateTriggerRule(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string chat_id = data["chat_id"];
    std::string chat_type = data["chat_type"];
    std::string rule_name = data["rule_name"];
    std::string trigger_conditions = data["trigger_conditions"];
    std::string actions = data["actions"];
    int rate_limit = data.count("rate_limit_per_second") ? std::stoi(data["rate_limit_per_second"]) : 1;
    
    if (token.empty() || chat_id.empty() || chat_type.empty() || rule_name.empty() || 
        trigger_conditions.empty() || actions.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Check permissions
    bool has_permission = false;
    if (chat_type == "group") {
        auto member = db_manager_.getGroupMember(chat_id, user_id);
        if (!member.id.empty() && (member.role == "admin" || member.role == "creator")) {
            has_permission = true;
        }
    } else if (chat_type == "channel") {
        auto member = db_manager_.getChannelMember(chat_id, user_id);
        if (!member.id.empty() && (member.role == "admin" || member.role == "creator")) {
            has_permission = true;
        }
    }
    
    if (!has_permission) {
        return JsonParser::createErrorResponse("Insufficient permissions");
    }
    
    std::string rule_id;
    if (db_manager_.createTriggerRule(chat_id, chat_type, rule_name, trigger_conditions, actions, rate_limit, user_id, rule_id)) {
        std::ostringstream oss;
        oss << "{\"success\":true,\"rule_id\":\"" << JsonParser::escapeJson(rule_id) << "\"}";
        return oss.str();
    }
    
    return JsonParser::createErrorResponse("Failed to create trigger rule");
}

std::string RequestHandler::handleUpdateTriggerRule(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string rule_id = data["rule_id"];
    std::string trigger_conditions = data["trigger_conditions"];
    std::string actions = data["actions"];
    bool is_active = data.count("is_active") ? (data["is_active"] == "true") : true;
    int rate_limit = data.count("rate_limit_per_second") ? std::stoi(data["rate_limit_per_second"]) : 1;
    
    if (token.empty() || rule_id.empty() || trigger_conditions.empty() || actions.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Security: BOLA fix - Check ownership of trigger rule
    auto rule = db_manager_.getTriggerRuleById(rule_id);
    if (!rule.has_value()) {
        return JsonParser::createErrorResponse("Trigger rule not found");
    }
    
    // Verify user has permission to modify this rule
    bool has_permission = false;
    if (rule->created_by == user_id) {
        has_permission = true;
    } else if (rule->chat_type == "group") {
        auto member = db_manager_.getGroupMember(rule->chat_id, user_id);
        if (!member.id.empty() && (member.role == "admin" || member.role == "creator")) {
            has_permission = true;
        }
    } else if (rule->chat_type == "channel") {
        auto member = db_manager_.getChannelMember(rule->chat_id, user_id);
        if (!member.id.empty() && (member.role == "admin" || member.role == "creator")) {
            has_permission = true;
        }
    }
    
    if (!has_permission) {
        Logger::getInstance().warning("BOLA attempt: User " + user_id + " tried to update trigger rule " + rule_id);
        return JsonParser::createErrorResponse("Permission denied");
    }
    
    if (db_manager_.updateTriggerRule(rule_id, trigger_conditions, actions, is_active, rate_limit)) {
        return JsonParser::createSuccessResponse("Trigger rule updated");
    }
    
    return JsonParser::createErrorResponse("Failed to update trigger rule");
}

std::string RequestHandler::handleDeleteTriggerRule(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string rule_id = data["rule_id"];
    
    if (token.empty() || rule_id.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Security: BOLA fix - Check ownership of trigger rule
    auto rule = db_manager_.getTriggerRuleById(rule_id);
    if (!rule.has_value()) {
        return JsonParser::createErrorResponse("Trigger rule not found");
    }
    
    // Verify user has permission to delete this rule
    bool has_permission = false;
    if (rule->created_by == user_id) {
        has_permission = true;
    } else if (rule->chat_type == "group") {
        auto member = db_manager_.getGroupMember(rule->chat_id, user_id);
        if (!member.id.empty() && (member.role == "admin" || member.role == "creator")) {
            has_permission = true;
        }
    } else if (rule->chat_type == "channel") {
        auto member = db_manager_.getChannelMember(rule->chat_id, user_id);
        if (!member.id.empty() && (member.role == "admin" || member.role == "creator")) {
            has_permission = true;
        }
    }
    
    if (!has_permission) {
        Logger::getInstance().warning("BOLA attempt: User " + user_id + " tried to delete trigger rule " + rule_id);
        return JsonParser::createErrorResponse("Permission denied");
    }
    
    if (db_manager_.deleteTriggerRule(rule_id)) {
        return JsonParser::createSuccessResponse("Trigger rule deleted");
    }
    
    return JsonParser::createErrorResponse("Failed to delete trigger rule");
}

std::string RequestHandler::handleGetTriggerRules(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string chat_id = data["chat_id"];
    std::string chat_type = data["chat_type"];
    
    if (chat_id.empty() || chat_type.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    auto rules = db_manager_.getTriggerRules(chat_id, chat_type);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"rules\":[";
    for (size_t i = 0; i < rules.size(); i++) {
        const auto& rule = rules[i];
        if (i > 0) oss << ",";
        oss << "{"
            << "\"id\":\"" << JsonParser::escapeJson(rule.id) << "\","
            << "\"rule_name\":\"" << JsonParser::escapeJson(rule.rule_name) << "\","
            << "\"is_active\":" << (rule.is_active ? "true" : "false") << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleCreateEventWebhook(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string trigger_rule_id = data["trigger_rule_id"];
    std::string webhook_url = data["webhook_url"];
    std::string secret_token = data.count("secret_token") ? data["secret_token"] : "";
    std::string http_method = data.count("http_method") ? data["http_method"] : "POST";
    std::string headers_json = data.count("headers_json") ? data["headers_json"] : "{}";
    
    if (token.empty() || trigger_rule_id.empty() || webhook_url.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    // Security: SSRF protection - Validate webhook URL
    // Block internal IPs, localhost, cloud metadata endpoints
    if (webhook_url.find("https://") != 0) {
        return JsonParser::createErrorResponse("Webhook URL must use HTTPS");
    }
    
    // Extract hostname for validation
    size_t host_start = 8; // After "https://"
    size_t host_end = webhook_url.find('/', host_start);
    if (host_end == std::string::npos) {
        host_end = webhook_url.length();
    }
    size_t port_pos = webhook_url.find(':', host_start);
    if (port_pos != std::string::npos && port_pos < host_end) {
        host_end = port_pos;
    }
    std::string hostname = webhook_url.substr(host_start, host_end - host_start);
    
    // Block internal/private addresses (SSRF protection)
    if (hostname == "localhost" || hostname == "127.0.0.1" || hostname == "::1" ||
        hostname.find("10.") == 0 || hostname.find("192.168.") == 0 ||
        hostname.find("169.254.") == 0 || hostname == "169.254.169.254" ||
        hostname == "metadata.google.internal" ||
        hostname.find(".internal") != std::string::npos ||
        hostname.find(".local") != std::string::npos) {
        Logger::getInstance().warning("SSRF attempt blocked: " + webhook_url);
        return JsonParser::createErrorResponse("Invalid webhook URL: internal addresses not allowed");
    }
    
    // Check for 172.16.0.0 - 172.31.255.255
    if (hostname.find("172.") == 0) {
        try {
            size_t dot2 = hostname.find('.', 4);
            if (dot2 != std::string::npos) {
                int second_octet = std::stoi(hostname.substr(4, dot2 - 4));
                if (second_octet >= 16 && second_octet <= 31) {
                    Logger::getInstance().warning("SSRF attempt blocked (172.x): " + webhook_url);
                    return JsonParser::createErrorResponse("Invalid webhook URL: internal addresses not allowed");
                }
            }
        } catch (...) {}
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    std::string webhook_id;
    if (db_manager_.createEventWebhook(trigger_rule_id, webhook_url, secret_token, http_method, headers_json, webhook_id)) {
        std::ostringstream oss;
        oss << "{\"success\":true,\"webhook_id\":\"" << JsonParser::escapeJson(webhook_id) << "\"}";
        return oss.str();
    }
    
    return JsonParser::createErrorResponse("Failed to create event webhook");
}

}

