#include "../../include/server/admin_handler.hpp"
#include "../../include/utils/json_parser.hpp"
#include "../../include/utils/logger.hpp"
#include "../../include/auth/password_hash.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <openssl/sha.h>
#include <openssl/rand.h>

namespace xipher {

namespace {
    const std::string* findHeaderValueCI(const std::map<std::string, std::string>& headers,
                                         const std::string& name) {
        auto it = headers.find(name);
        if (it != headers.end()) {
            return &it->second;
        }
        std::string target = name;
        std::transform(target.begin(), target.end(), target.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        for (const auto& kv : headers) {
            std::string key = kv.first;
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (key == target) {
                return &kv.second;
            }
        }
        return nullptr;
    }

    std::string extractCookie(const std::string& cookie_header, const std::string& name) {
        std::string needle = name + "=";
        size_t pos = cookie_header.find(needle);
        if (pos == std::string::npos) return "";
        size_t start = pos + needle.size();
        size_t end = cookie_header.find(';', start);
        return cookie_header.substr(start, end == std::string::npos ? cookie_header.size() - start : end - start);
    }

    std::string randomToken() {
        unsigned char bytes[32];
        if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
            return "";
        }
        std::ostringstream oss;
        for (unsigned char b : bytes) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        return oss.str();
    }

    std::string extractSessionToken(const std::map<std::string, std::string>& headers) {
        const std::string* cookie_value = findHeaderValueCI(headers, "Cookie");
        if (!cookie_value) {
            return "";
        }
        return extractCookie(*cookie_value, "xipher_token");
    }

    std::string trimCopy(const std::string& input) {
        const size_t start = input.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            return "";
        }
        const size_t end = input.find_last_not_of(" \t\r\n");
        return input.substr(start, end - start + 1);
    }

    std::string firstForwardedIp(const std::string& header_value) {
        const size_t comma = header_value.find(',');
        const std::string first = (comma == std::string::npos) ? header_value : header_value.substr(0, comma);
        return trimCopy(first);
    }

    std::string extractClientIp(const std::map<std::string, std::string>& headers) {
        const std::string* cf_ip = findHeaderValueCI(headers, "CF-Connecting-IP");
        if (cf_ip) {
            const std::string value = trimCopy(*cf_ip);
            if (!value.empty()) {
                return value;
            }
        }

        const std::string* true_client = findHeaderValueCI(headers, "True-Client-IP");
        if (true_client) {
            const std::string value = trimCopy(*true_client);
            if (!value.empty()) {
                return value;
            }
        }

        const std::string* forwarded_for = findHeaderValueCI(headers, "X-Forwarded-For");
        if (forwarded_for) {
            const std::string value = firstForwardedIp(*forwarded_for);
            if (!value.empty()) {
                return value;
            }
        }

        const std::string* real_ip = findHeaderValueCI(headers, "X-Real-IP");
        if (real_ip) {
            const std::string value = trimCopy(*real_ip);
            if (!value.empty()) {
                return value;
            }
        }

        const std::string* client_ip = findHeaderValueCI(headers, "X-Client-IP");
        if (client_ip) {
            return trimCopy(*client_ip);
        }

        return "";
    }

    std::string toLowerCopy(const std::string& input) {
        std::string out = input;
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

    bool isSecureRequest(const std::map<std::string, std::string>& headers) {
        const std::string* proto = findHeaderValueCI(headers, "X-Forwarded-Proto");
        if (proto && toLowerCopy(*proto).find("https") != std::string::npos) {
            return true;
        }
        const std::string* scheme = findHeaderValueCI(headers, "X-Forwarded-Scheme");
        if (scheme && toLowerCopy(*scheme).find("https") != std::string::npos) {
            return true;
        }
        const std::string* ssl = findHeaderValueCI(headers, "X-Forwarded-SSL");
        if (ssl) {
            const std::string value = toLowerCopy(*ssl);
            if (value == "on" || value == "1" || value == "true") {
                return true;
            }
        }
        const std::string* forwarded = findHeaderValueCI(headers, "Forwarded");
        if (forwarded && toLowerCopy(*forwarded).find("proto=https") != std::string::npos) {
            return true;
        }
        const std::string* origin = findHeaderValueCI(headers, "Origin");
        if (origin && toLowerCopy(*origin).rfind("https://", 0) == 0) {
            return true;
        }
        return false;
    }

    bool parseBool(const std::string& value) {
        const std::string lower = toLowerCopy(value);
        return lower == "true" || lower == "1" || lower == "yes" || lower == "on";
    }

    int parseIntOrDefault(const std::string& value, int fallback) {
        try {
            return std::stoi(value);
        } catch (...) {
            return fallback;
        }
    }

    int clampInt(int value, int min_value, int max_value) {
        if (value < min_value) return min_value;
        if (value > max_value) return max_value;
        return value;
    }
}

AdminHandler::AdminHandler(DatabaseManager& db_manager, AuthManager& auth_manager)
    : db_manager_(db_manager),
      auth_manager_(auth_manager),
      login_rate_limiter_(3, 60, 24 * 60 * 60),
      shadow_ban_rate_limiter_(1, 60, 24 * 60 * 60) {}

std::string AdminHandler::hashContext(const std::string& input) const {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    std::ostringstream oss;
    for (unsigned char b : hash) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

std::string AdminHandler::buildHttpJson(const std::string& body, const std::map<std::string, std::string>& extra_headers) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Type: application/json\r\n";
    oss << "Cache-Control: no-store\r\n";
    for (const auto& kv : extra_headers) {
        oss << kv.first << ": " << kv.second << "\r\n";
    }
    oss << "Content-Length: " << body.size() << "\r\n\r\n";
    oss << body;
    return oss.str();
}

bool AdminHandler::validateAdminSession(const std::string& token, const std::string& ip, const std::string& user_agent) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = admin_sessions_.find(token);
    if (it == admin_sessions_.end()) {
        return false;
    }
    auto& session = it->second;
    if (!session.is_2fa_verified) {
        return false;
    }
    if (!session.ip_hash.empty()) {
        if (ip.empty() || session.ip_hash != hashContext(ip)) {
            admin_sessions_.erase(it);
            return false;
        }
    }
    if (!session.user_agent_hash.empty()) {
        if (user_agent.empty() || session.user_agent_hash != hashContext(user_agent)) {
            admin_sessions_.erase(it);
            return false;
        }
    }
    const auto ttl = std::chrono::minutes(30);
    if (now - session.created_at > ttl) {
        admin_sessions_.erase(it);
        return false;
    }
    return true;
}

std::string AdminHandler::handleLogin(const std::map<std::string, std::string>& headers, const std::string& body) {
    auto data = JsonParser::parse(body);
    const std::string ip = extractClientIp(headers);
    const std::string* user_agent_header = findHeaderValueCI(headers, "User-Agent");
    const std::string user_agent = user_agent_header ? *user_agent_header : "";
    const std::string key = ip.empty() ? "unknown" : ip;

    std::string ban_reason;
    if (!login_rate_limiter_.allow(key, ban_reason)) {
        Logger::getInstance().warning("Admin login rate limited for IP: " + key);
        return buildHttpJson(JsonParser::createErrorResponse("Too many attempts - temporarily blocked"));
    }

    const std::string username = data.count("username") ? data.at("username") : "";
    if (username.empty()) {
        return buildHttpJson(JsonParser::createErrorResponse("Username required"));
    }
    
    // Security: Admin username from environment variable
    const char* admin_user_env = std::getenv("XIPHER_ADMIN_USERNAME");
    const std::string admin_username = admin_user_env ? admin_user_env : "prd";
    if (username != admin_username) {
        return buildHttpJson(JsonParser::createErrorResponse("Access denied"));
    }

    const std::string session_token = extractSessionToken(headers);
    const std::string session_user_id = session_token.empty() ? "" : auth_manager_.getUserIdFromToken(session_token);
    if (session_user_id.empty()) {
        return buildHttpJson(JsonParser::createErrorResponse("Not authorized"));
    }

    const User session_user = db_manager_.getUserById(session_user_id);
    if (session_user.id.empty() || session_user.username != "prd") {
        return buildHttpJson(JsonParser::createErrorResponse("Not authorized"));
    }

    User user = db_manager_.getUserByUsername(username);
    if (user.id.empty() || user.id != session_user.id || (user.role != "admin" && user.role != "owner" && !user.is_admin)) {
        return buildHttpJson(JsonParser::createErrorResponse("Not authorized"));
    }

    const std::string password = data.count("password") ? data.at("password") : "";
    if (!PasswordHash::verify(password, user.password_hash)) {
        return buildHttpJson(JsonParser::createErrorResponse("Invalid credentials"));
    }

    const std::string token = randomToken();
    if (token.empty()) {
        return buildHttpJson(JsonParser::createErrorResponse("Failed to create admin session"));
    }
    AdminSessionInfo session;
    session.token = token;
    session.user_id = user.id;
    session.username = user.username;
    session.ip_hash = ip.empty() ? "" : hashContext(ip);
    session.user_agent_hash = user_agent.empty() ? "" : hashContext(user_agent);
    session.created_at = std::chrono::steady_clock::now();
    session.is_2fa_verified = true;

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        admin_sessions_[token] = session;
    }

    login_rate_limiter_.clear(key);

    std::map<std::string, std::string> headers_out;
    std::ostringstream cookie;
    cookie << "AdminToken=" << token << "; HttpOnly; SameSite=Strict; Path=/; Max-Age=1800";
    if (isSecureRequest(headers)) {
        cookie << "; Secure";
    }
    headers_out["Set-Cookie"] = cookie.str();

    std::map<std::string, std::string> response_data;
    response_data["status"] = "ok";
    response_data["username"] = user.username;
    response_data["role"] = user.role;
    response_data["token"] = token;

    return buildHttpJson(JsonParser::createSuccessResponse("Admin session issued", response_data), headers_out);
}

std::string AdminHandler::handleAction(const std::map<std::string, std::string>& headers, const std::string& body) {
    const std::string ip = extractClientIp(headers);
    const std::string* user_agent_header = findHeaderValueCI(headers, "User-Agent");
    const std::string user_agent = user_agent_header ? *user_agent_header : "";
    const std::string* cookie_header_value = findHeaderValueCI(headers, "Cookie");
    const std::string cookie_header = cookie_header_value ? *cookie_header_value : "";
    std::string token = extractCookie(cookie_header, "AdminToken");
    if (token.empty() && headers.count("Authorization")) {
        const std::string auth = headers.at("Authorization");
        if (auth.rfind("Bearer ", 0) == 0) {
            token = auth.substr(7);
        }
    }

    if (token.empty()) {
        std::string ban_reason;
        shadow_ban_rate_limiter_.allow(ip, ban_reason);
        return buildHttpJson(JsonParser::createErrorResponse("Missing admin token"));
    }

    if (!validateAdminSession(token, ip, user_agent)) {
        std::string ban_reason;
        shadow_ban_rate_limiter_.allow(ip, ban_reason);
        return buildHttpJson(JsonParser::createErrorResponse("Invalid admin session"));
    }

    const std::string session_token = extractSessionToken(headers);
    const std::string session_user_id = session_token.empty() ? "" : auth_manager_.getUserIdFromToken(session_token);
    if (session_user_id.empty()) {
        return buildHttpJson(JsonParser::createErrorResponse("Not authorized"));
    }
    const User session_user = db_manager_.getUserById(session_user_id);
    // Security: Admin username from environment variable
    const char* admin_env2 = std::getenv("XIPHER_ADMIN_USERNAME");
    const std::string expected_admin2 = admin_env2 ? admin_env2 : "prd";
    if (session_user.id.empty() || session_user.username != expected_admin2) {
        return buildHttpJson(JsonParser::createErrorResponse("Not authorized"));
    }

    auto data = JsonParser::parse(body);
    const std::string action = data["action"];

    if (action == "search") {
        const std::string query = data.count("query") ? data.at("query") : "";
        const std::string scope = data.count("scope") ? data.at("scope") : "all";
        int limit = parseIntOrDefault(data.count("limit") ? data.at("limit") : "", 20);
        limit = clampInt(limit, 1, 50);

        const bool need_all = scope == "all";
        const bool need_users = need_all || scope == "users";
        const bool need_messages = need_all || scope == "messages";
        const bool need_groups = need_all || scope == "groups";
        const bool need_channels = need_all || scope == "channels";

        std::ostringstream oss;
        oss << "{\"success\":true,\"query\":\"" << JsonParser::escapeJson(query) << "\"";

        if (need_users) {
            auto users = db_manager_.searchUsers(query, limit);
            oss << ",\"users\":[";
            bool first = true;
            for (const auto& u : users) {
                if (!first) oss << ",";
                first = false;
                oss << "{"
                    << "\"id\":\"" << JsonParser::escapeJson(u.id) << "\","
                    << "\"username\":\"" << JsonParser::escapeJson(u.username) << "\","
                    << "\"is_active\":" << (u.is_active ? "true" : "false") << ","
                    << "\"is_admin\":" << (u.is_admin ? "true" : "false") << ","
                    << "\"is_bot\":" << (u.is_bot ? "true" : "false") << ","
                    << "\"role\":\"" << JsonParser::escapeJson(u.role) << "\","
                    << "\"last_login\":\"" << JsonParser::escapeJson(u.last_login) << "\","
                    << "\"last_activity\":\"" << JsonParser::escapeJson(u.last_activity) << "\","
                    << "\"created_at\":\"" << JsonParser::escapeJson(u.created_at) << "\""
                    << "}";
            }
            oss << "]";
        }

        if (need_messages) {
            auto messages = db_manager_.searchMessages(query, limit);
            oss << ",\"messages\":[";
            bool first = true;
            for (const auto& m : messages) {
                if (!first) oss << ",";
                first = false;
                oss << "{"
                    << "\"id\":\"" << JsonParser::escapeJson(m.id) << "\","
                    << "\"sender_id\":\"" << JsonParser::escapeJson(m.sender_id) << "\","
                    << "\"sender_username\":\"" << JsonParser::escapeJson(m.sender_username) << "\","
                    << "\"receiver_id\":\"" << JsonParser::escapeJson(m.receiver_id) << "\","
                    << "\"receiver_username\":\"" << JsonParser::escapeJson(m.receiver_username) << "\","
                    << "\"content\":\"" << JsonParser::escapeJson(m.content) << "\","
                    << "\"message_type\":\"" << JsonParser::escapeJson(m.message_type) << "\","
                    << "\"created_at\":\"" << JsonParser::escapeJson(m.created_at) << "\""
                    << "}";
            }
            oss << "]";
        }

        if (need_groups) {
            auto groups = db_manager_.searchGroups(query, limit);
            oss << ",\"groups\":[";
            bool first = true;
            for (const auto& g : groups) {
                if (!first) oss << ",";
                first = false;
                oss << "{"
                    << "\"id\":\"" << JsonParser::escapeJson(g.id) << "\","
                    << "\"name\":\"" << JsonParser::escapeJson(g.name) << "\","
                    << "\"description\":\"" << JsonParser::escapeJson(g.description) << "\","
                    << "\"creator_id\":\"" << JsonParser::escapeJson(g.creator_id) << "\","
                    << "\"invite_link\":\"" << JsonParser::escapeJson(g.invite_link) << "\","
                    << "\"created_at\":\"" << JsonParser::escapeJson(g.created_at) << "\""
                    << "}";
            }
            oss << "]";
        }

        if (need_channels) {
            auto channels = db_manager_.searchChannels(query, limit);
            oss << ",\"channels\":[";
            bool first = true;
            for (const auto& c : channels) {
                if (!first) oss << ",";
                first = false;
                oss << "{"
                    << "\"id\":\"" << JsonParser::escapeJson(c.id) << "\","
                    << "\"name\":\"" << JsonParser::escapeJson(c.name) << "\","
                    << "\"description\":\"" << JsonParser::escapeJson(c.description) << "\","
                    << "\"creator_id\":\"" << JsonParser::escapeJson(c.creator_id) << "\","
                    << "\"custom_link\":\"" << JsonParser::escapeJson(c.custom_link) << "\","
                    << "\"created_at\":\"" << JsonParser::escapeJson(c.created_at) << "\""
                    << "}";
            }
            oss << "]";
        }

        oss << "}";
        return buildHttpJson(oss.str());
    }

    if (action == "user_lookup") {
        const std::string query = data.count("query") ? data.at("query") : "";
        auto users = db_manager_.searchUsers(query, 1);
        if (users.empty()) {
            return buildHttpJson(JsonParser::createErrorResponse("User not found"));
        }
        const auto& u = users.front();
        std::map<std::string, std::string> payload;
        payload["id"] = u.id;
        payload["username"] = u.username;
        payload["role"] = u.role;
        payload["is_active"] = u.is_active ? "true" : "false";
        payload["is_admin"] = u.is_admin ? "true" : "false";
        payload["is_bot"] = u.is_bot ? "true" : "false";
        payload["last_login"] = u.last_login;
        payload["last_activity"] = u.last_activity;
        payload["created_at"] = u.created_at;
        return buildHttpJson(JsonParser::createSuccessResponse("User found", payload));
    }

    if (action == "user_set_role") {
        const std::string user_id = data.count("user_id") ? data.at("user_id") : "";
        const std::string role = data.count("role") ? data.at("role") : "";
        if (user_id.empty() || role.empty()) {
            return buildHttpJson(JsonParser::createErrorResponse("user_id and role required"));
        }
        if (!db_manager_.setUserRole(user_id, role)) {
            return buildHttpJson(JsonParser::createErrorResponse("Failed to update role"));
        }
        return buildHttpJson(JsonParser::createSuccessResponse("Role updated"));
    }

    if (action == "user_set_admin") {
        const std::string user_id = data.count("user_id") ? data.at("user_id") : "";
        const std::string flag = data.count("is_admin") ? data.at("is_admin") : "";
        if (user_id.empty() || flag.empty()) {
            return buildHttpJson(JsonParser::createErrorResponse("user_id and is_admin required"));
        }
        if (!db_manager_.setUserAdminFlag(user_id, parseBool(flag))) {
            return buildHttpJson(JsonParser::createErrorResponse("Failed to update admin flag"));
        }
        return buildHttpJson(JsonParser::createSuccessResponse("Admin flag updated"));
    }

    if (action == "user_force_logout") {
        const std::string user_id = data.count("user_id") ? data.at("user_id") : "";
        if (user_id.empty()) {
            return buildHttpJson(JsonParser::createErrorResponse("user_id required"));
        }
        if (!db_manager_.deleteUserSessionsByUser(user_id)) {
            return buildHttpJson(JsonParser::createErrorResponse("Failed to revoke sessions"));
        }
        return buildHttpJson(JsonParser::createSuccessResponse("Sessions revoked"));
    }

    if (action == "delete_message_any") {
        const std::string message_id = data.count("message_id") ? data.at("message_id") : "";
        const std::string message_type = data.count("message_type") ? data.at("message_type") : "";
        if (message_id.empty()) {
            return buildHttpJson(JsonParser::createErrorResponse("message_id required"));
        }
        bool deleted = false;
        std::string final_type = message_type;

        if (!deleted && (message_type.empty() || message_type == "direct")) {
            deleted = db_manager_.deleteMessageById(message_id);
            if (deleted) final_type = "direct";
        }
        if (!deleted && (message_type.empty() || message_type == "group")) {
            std::string group_id;
            std::string sender_id;
            if (db_manager_.getGroupMessageMeta(message_id, group_id, sender_id)) {
                deleted = db_manager_.deleteGroupMessage(group_id, message_id);
                if (deleted) final_type = "group";
            }
        }
        if (!deleted && (message_type.empty() || message_type == "channel")) {
            std::string channel_id;
            std::string sender_id;
            if (db_manager_.getChannelMessageMetaLegacy(message_id, channel_id, sender_id)) {
                deleted = db_manager_.deleteChannelMessageLegacy(channel_id, message_id);
                if (deleted) final_type = "channel";
            }
        }
        if (!deleted && (message_type.empty() || message_type == "chat_v2")) {
            std::string chat_id;
            std::string sender_id;
            if (db_manager_.getChatMessageMetaV2(message_id, chat_id, sender_id)) {
                deleted = db_manager_.deleteChatMessageV2(chat_id, message_id);
                if (deleted) final_type = "chat_v2";
            }
        }

        if (!deleted) {
            return buildHttpJson(JsonParser::createErrorResponse("Failed to delete message"));
        }

        std::map<std::string, std::string> payload;
        payload["message_type"] = final_type;
        return buildHttpJson(JsonParser::createSuccessResponse("Message deleted", payload));
    }

    if (action == "auto_moderation_get") {
        std::map<std::string, std::string> payload;
        payload["enabled"] = db_manager_.getAdminSetting("auto_mod_enabled");
        payload["spam_threshold"] = db_manager_.getAdminSetting("auto_mod_spam_threshold");
        payload["abuse_threshold"] = db_manager_.getAdminSetting("auto_mod_abuse_threshold");
        payload["delete_on_threshold"] = db_manager_.getAdminSetting("auto_mod_delete_enabled");
        payload["ban_on_threshold"] = db_manager_.getAdminSetting("auto_mod_ban_enabled");
        payload["ban_hours"] = db_manager_.getAdminSetting("auto_mod_ban_hours");
        payload["window_minutes"] = db_manager_.getAdminSetting("auto_mod_window_minutes");
        payload["auto_resolve"] = db_manager_.getAdminSetting("auto_mod_auto_resolve");
        return buildHttpJson(JsonParser::createSuccessResponse("Auto moderation settings", payload));
    }

    if (action == "auto_moderation_set") {
        const std::string enabled = data.count("enabled") ? data.at("enabled") : "false";
        const std::string spam_threshold = data.count("spam_threshold") ? data.at("spam_threshold") : "3";
        const std::string abuse_threshold = data.count("abuse_threshold") ? data.at("abuse_threshold") : "2";
        const std::string delete_on_threshold = data.count("delete_on_threshold") ? data.at("delete_on_threshold") : "true";
        const std::string ban_on_threshold = data.count("ban_on_threshold") ? data.at("ban_on_threshold") : "true";
        const std::string ban_hours = data.count("ban_hours") ? data.at("ban_hours") : "24";
        const std::string window_minutes = data.count("window_minutes") ? data.at("window_minutes") : "60";
        const std::string auto_resolve = data.count("auto_resolve") ? data.at("auto_resolve") : "true";

        const int spam_value = clampInt(parseIntOrDefault(spam_threshold, 3), 1, 20);
        const int abuse_value = clampInt(parseIntOrDefault(abuse_threshold, 2), 1, 20);
        const int ban_hours_value = clampInt(parseIntOrDefault(ban_hours, 24), 0, 720);
        const int window_value = clampInt(parseIntOrDefault(window_minutes, 60), 5, 1440);

        db_manager_.upsertAdminSetting("auto_mod_enabled", parseBool(enabled) ? "true" : "false");
        db_manager_.upsertAdminSetting("auto_mod_spam_threshold", std::to_string(spam_value));
        db_manager_.upsertAdminSetting("auto_mod_abuse_threshold", std::to_string(abuse_value));
        db_manager_.upsertAdminSetting("auto_mod_delete_enabled", parseBool(delete_on_threshold) ? "true" : "false");
        db_manager_.upsertAdminSetting("auto_mod_ban_enabled", parseBool(ban_on_threshold) ? "true" : "false");
        db_manager_.upsertAdminSetting("auto_mod_ban_hours", std::to_string(ban_hours_value));
        db_manager_.upsertAdminSetting("auto_mod_window_minutes", std::to_string(window_value));
        db_manager_.upsertAdminSetting("auto_mod_auto_resolve", parseBool(auto_resolve) ? "true" : "false");

        return buildHttpJson(JsonParser::createSuccessResponse("Auto moderation updated"));
    }

    if (action == "premium_payment_lookup") {
        const std::string label = data.count("label") ? data.at("label") : "";
        if (label.empty()) {
            return buildHttpJson(JsonParser::createErrorResponse("label required"));
        }
        PremiumPayment payment;
        if (!db_manager_.getPremiumPaymentByLabel(label, payment)) {
            return buildHttpJson(JsonParser::createErrorResponse("Payment not found"));
        }
        User user = db_manager_.getUserById(payment.user_id);
        std::map<std::string, std::string> payload;
        payload["label"] = payment.label;
        payload["status"] = payment.status;
        payload["plan"] = payment.plan;
        payload["amount"] = payment.amount;
        payload["operation_id"] = payment.operation_id;
        payload["payment_type"] = payment.payment_type;
        payload["created_at"] = payment.created_at;
        payload["paid_at"] = payment.paid_at;
        payload["user_id"] = payment.user_id;
        payload["username"] = user.username;
        payload["user_is_premium"] = user.is_premium ? "true" : "false";
        payload["user_premium_plan"] = user.premium_plan;
        payload["user_premium_expires_at"] = user.premium_expires_at;
        return buildHttpJson(JsonParser::createSuccessResponse("Payment found", payload));
    }

    if (action == "premium_payment_list") {
        const std::string status = data.count("status") ? data.at("status") : "pending";
        int limit = parseIntOrDefault(data.count("limit") ? data.at("limit") : "", 20);
        limit = clampInt(limit, 1, 100);
        auto payments = db_manager_.listPremiumPayments(status, limit);
        std::ostringstream oss;
        oss << "{\"success\":true,\"status\":\"" << JsonParser::escapeJson(status) << "\","
            << "\"count\":" << payments.size() << ",\"payments\":[";
        bool first = true;
        for (const auto& p : payments) {
            if (!first) oss << ",";
            first = false;
            oss << "{"
                << "\"id\":\"" << JsonParser::escapeJson(p.id) << "\","
                << "\"label\":\"" << JsonParser::escapeJson(p.label) << "\","
                << "\"plan\":\"" << JsonParser::escapeJson(p.plan) << "\","
                << "\"amount\":\"" << JsonParser::escapeJson(p.amount) << "\","
                << "\"status\":\"" << JsonParser::escapeJson(p.status) << "\","
                << "\"operation_id\":\"" << JsonParser::escapeJson(p.operation_id) << "\","
                << "\"payment_type\":\"" << JsonParser::escapeJson(p.payment_type) << "\","
                << "\"created_at\":\"" << JsonParser::escapeJson(p.created_at) << "\","
                << "\"paid_at\":\"" << JsonParser::escapeJson(p.paid_at) << "\","
                << "\"user_id\":\"" << JsonParser::escapeJson(p.user_id) << "\","
                << "\"username\":\"" << JsonParser::escapeJson(p.username) << "\""
                << "}";
        }
        oss << "]}";
        return buildHttpJson(oss.str());
    }

    if (action == "premium_payment_confirm") {
        const std::string label = data.count("label") ? data.at("label") : "";
        if (label.empty()) {
            return buildHttpJson(JsonParser::createErrorResponse("label required"));
        }
        PremiumPayment payment;
        if (!db_manager_.getPremiumPaymentByLabel(label, payment)) {
            return buildHttpJson(JsonParser::createErrorResponse("Payment not found"));
        }
        if (payment.status == "paid") {
            return buildHttpJson(JsonParser::createErrorResponse("Payment already marked as paid"));
        }
        std::string operation_id = data.count("operation_id") ? data.at("operation_id") : "";
        std::string payment_type = data.count("payment_type") ? data.at("payment_type") : "";
        if (operation_id.empty()) operation_id = "manual";
        if (payment_type.empty()) payment_type = "manual";
        if (!db_manager_.markPremiumPaymentPaid(label, operation_id, payment_type)) {
            return buildHttpJson(JsonParser::createErrorResponse("Failed to mark payment as paid"));
        }
        if (!db_manager_.setUserPremium(payment.user_id, true, payment.plan)) {
            db_manager_.resetPremiumPaymentStatus(label);
            return buildHttpJson(JsonParser::createErrorResponse("Failed to activate premium"));
        }
        User updated = db_manager_.getUserById(payment.user_id);
        std::map<std::string, std::string> payload;
        payload["user_id"] = payment.user_id;
        payload["username"] = updated.username;
        payload["plan"] = payment.plan;
        payload["premium_expires_at"] = updated.premium_expires_at;
        return buildHttpJson(JsonParser::createSuccessResponse("Premium activated", payload));
    }

    if (action == "premium_payment_reactivate") {
        const std::string label = data.count("label") ? data.at("label") : "";
        if (label.empty()) {
            return buildHttpJson(JsonParser::createErrorResponse("label required"));
        }
        PremiumPayment payment;
        if (!db_manager_.getPremiumPaymentByLabel(label, payment)) {
            return buildHttpJson(JsonParser::createErrorResponse("Payment not found"));
        }
        if (payment.status != "paid") {
            return buildHttpJson(JsonParser::createErrorResponse("Payment is not marked as paid"));
        }
        User user = db_manager_.getUserById(payment.user_id);
        if (user.id.empty()) {
            return buildHttpJson(JsonParser::createErrorResponse("User not found"));
        }
        if (user.is_premium) {
            return buildHttpJson(JsonParser::createErrorResponse("User already has premium"));
        }
        if (!db_manager_.setUserPremium(payment.user_id, true, payment.plan)) {
            return buildHttpJson(JsonParser::createErrorResponse("Failed to reactivate premium"));
        }
        User updated = db_manager_.getUserById(payment.user_id);
        std::map<std::string, std::string> payload;
        payload["user_id"] = payment.user_id;
        payload["username"] = updated.username;
        payload["plan"] = payment.plan;
        payload["premium_expires_at"] = updated.premium_expires_at;
        return buildHttpJson(JsonParser::createSuccessResponse("Premium reactivated", payload));
    }

    if (action == "ban_user") {
        if (!db_manager_.setUserActive(data["user_id"], false)) {
            return buildHttpJson(JsonParser::createErrorResponse("Failed to ban user"));
        }
        return buildHttpJson(JsonParser::createSuccessResponse("User banned"));
    }

    if (action == "unban_user") {
        if (!db_manager_.setUserActive(data["user_id"], true)) {
            return buildHttpJson(JsonParser::createErrorResponse("Failed to unban user"));
        }
        return buildHttpJson(JsonParser::createSuccessResponse("User unbanned"));
    }

    if (action == "reset_password") {
        std::string new_hash = PasswordHash::hash(data["new_password"]);
        if (new_hash.empty()) {
            return buildHttpJson(JsonParser::createErrorResponse("Failed to hash password"));
        }
        if (!db_manager_.updateUserPasswordHash(data["user_id"], new_hash)) {
            return buildHttpJson(JsonParser::createErrorResponse("Failed to reset password"));
        }
        return buildHttpJson(JsonParser::createSuccessResponse("Password reset"));
    }

    if (action == "delete_message") {
        if (!db_manager_.deleteMessageById(data["message_id"])) {
            return buildHttpJson(JsonParser::createErrorResponse("Failed to delete message"));
        }
        return buildHttpJson(JsonParser::createSuccessResponse("Message deleted"));
    }

    if (action == "nuke_user_messages") {
        if (!db_manager_.deleteMessagesByUser(data["user_id"])) {
            return buildHttpJson(JsonParser::createErrorResponse("Failed to purge messages"));
        }
        return buildHttpJson(JsonParser::createSuccessResponse("Messages purged"));
    }

    if (action == "stats") {
        std::map<std::string, std::string> stats;
        stats["users"] = std::to_string(db_manager_.countUsers());
        stats["messages_today"] = std::to_string(db_manager_.countMessagesToday());
        stats["online_users"] = std::to_string(db_manager_.countOnlineUsers());
        stats["banned_users"] = std::to_string(db_manager_.countBannedUsers());
        stats["active_24h"] = std::to_string(db_manager_.countActiveUsersSince(24 * 3600));
        stats["groups"] = std::to_string(db_manager_.countGroups());
        stats["channels"] = std::to_string(db_manager_.countChannels());
        stats["bots"] = std::to_string(db_manager_.countBots());
        stats["reports_pending"] = std::to_string(db_manager_.countReportsByStatus("pending"));
        return buildHttpJson(JsonParser::createSuccessResponse("stats", stats));
    }

    if (action == "broadcast") {
        Logger::getInstance().info("Admin broadcast requested (implement WebSocket fanout separately)");
        return buildHttpJson(JsonParser::createSuccessResponse("Broadcast accepted"));
    }

    if (action == "restart") {
        Logger::getInstance().warning("Admin restart requested (placeholder - hook into supervisor)");
        return buildHttpJson(JsonParser::createSuccessResponse("Restart queued"));
    }

    return buildHttpJson(JsonParser::createErrorResponse("Unknown action"));
}

} // namespace xipher
