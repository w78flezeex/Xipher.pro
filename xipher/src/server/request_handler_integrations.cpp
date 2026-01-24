// Integration API handlers
// OWASP 2025 Security Compliant

#include "../include/server/request_handler.hpp"
#include "../include/utils/json_parser.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>

namespace xipher {

std::string RequestHandler::handleCreateIntegration(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string chat_id = data["chat_id"];
    std::string chat_type = data["chat_type"];
    std::string service_name = data["service_name"];
    std::string external_token = data["external_token"];
    std::string config_json = data.count("config_json") ? data["config_json"] : "{}";
    
    if (token.empty() || chat_id.empty() || chat_type.empty() || service_name.empty() || external_token.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    // Validate chat_type
    if (chat_type != "group" && chat_type != "channel") {
        return JsonParser::createErrorResponse("Invalid chat_type");
    }
    
    // Validate service_name
    if (service_name != "discord" && service_name != "github" && service_name != "svortex" && 
        service_name != "trello" && service_name != "rss") {
        return JsonParser::createErrorResponse("Invalid service_name");
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
    
    std::string binding_id;
    if (db_manager_.createIntegrationBinding(chat_id, chat_type, service_name, external_token, config_json, user_id, binding_id)) {
        std::ostringstream oss;
        oss << "{\"success\":true,\"binding_id\":\"" << JsonParser::escapeJson(binding_id) << "\"}";
        return oss.str();
    }
    
    return JsonParser::createErrorResponse("Failed to create integration");
}

std::string RequestHandler::handleGetIntegrations(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string chat_id = data["chat_id"];
    std::string chat_type = data["chat_type"];
    
    if (chat_id.empty() || chat_type.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    // Security: Require authentication
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Security: BOLA fix - Verify user is a member of the chat
    bool is_member = false;
    if (chat_type == "group") {
        auto member = db_manager_.getGroupMember(chat_id, user_id);
        is_member = !member.id.empty();
    } else if (chat_type == "channel") {
        auto member = db_manager_.getChannelMember(chat_id, user_id);
        is_member = !member.id.empty();
    }
    
    if (!is_member) {
        return JsonParser::createErrorResponse("Permission denied");
    }
    
    auto integrations = db_manager_.getIntegrationBindings(chat_id, chat_type);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"integrations\":[";
    for (size_t i = 0; i < integrations.size(); i++) {
        const auto& integration = integrations[i];
        if (i > 0) oss << ",";
        oss << "{"
            << "\"id\":\"" << JsonParser::escapeJson(integration.id) << "\","
            << "\"service_name\":\"" << JsonParser::escapeJson(integration.service_name) << "\","
            << "\"is_active\":" << (integration.is_active ? "true" : "false") << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleDeleteIntegration(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string integration_id = data["integration_id"];
    
    if (token.empty() || integration_id.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Check ownership
    auto integration = db_manager_.getIntegrationBindingById(integration_id);
    if (integration.id.empty()) {
        return JsonParser::createErrorResponse("Integration not found");
    }
    
    if (integration.created_by != user_id) {
        return JsonParser::createErrorResponse("Insufficient permissions");
    }
    
    if (db_manager_.deleteIntegrationBinding(integration_id)) {
        return JsonParser::createSuccessResponse("Integration deleted");
    }
    
    return JsonParser::createErrorResponse("Failed to delete integration");
}

std::string RequestHandler::handleOAuthCallback(const std::string& /*path*/, const std::string& /*body*/) {
    // TODO: Реализовать OAuth callback обработку
    return JsonParser::createErrorResponse("OAuth callback not implemented yet");
}

}

