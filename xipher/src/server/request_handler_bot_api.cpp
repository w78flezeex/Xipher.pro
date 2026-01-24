#include "../include/server/request_handler.hpp"
#include "../include/storage/in_memory_storage.hpp"
#include "../include/utils/json_parser.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <vector>

namespace xipher {

// Helper: Parse query string parameters
std::map<std::string, std::string> parseQueryString(const std::string& query) {
    std::map<std::string, std::string> params;
    if (query.empty()) return params;
    
    size_t pos = 0;
    while (pos < query.length()) {
        size_t eq_pos = query.find('=', pos);
        if (eq_pos == std::string::npos) break;
        
        size_t amp_pos = query.find('&', eq_pos);
        if (amp_pos == std::string::npos) amp_pos = query.length();
        
        std::string key = query.substr(pos, eq_pos - pos);
        std::string value = query.substr(eq_pos + 1, amp_pos - eq_pos - 1);
        
        // URL decode (simplified)
        params[key] = value;
        
        pos = amp_pos + 1;
    }
    
    return params;
}

std::string trimString(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        start++;
    }
    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        end--;
    }
    return value.substr(start, end - start);
}

std::string extractJsonValueForKey(const std::string& body, const std::string& key) {
    if (body.empty()) return "";
    const std::string needle = "\"" + key + "\"";
    size_t pos = body.find(needle);
    if (pos == std::string::npos) return "";
    pos = body.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) {
        pos++;
    }
    if (pos >= body.size()) return "";

    const char first = body[pos];
    if (first == '"') {
        std::string raw;
        bool escape = false;
        for (size_t i = pos + 1; i < body.size(); i++) {
            char c = body[i];
            if (escape) {
                raw.push_back(c);
                escape = false;
                continue;
            }
            if (c == '\\') {
                raw.push_back(c);
                escape = true;
                continue;
            }
            if (c == '"') {
                return JsonParser::unescapeJson(raw);
            }
            raw.push_back(c);
        }
        return "";
    }

    if (first == '{' || first == '[') {
        std::vector<char> stack;
        bool inString = false;
        bool escape = false;
        for (size_t i = pos; i < body.size(); i++) {
            char c = body[i];
            if (escape) {
                escape = false;
                continue;
            }
            if (c == '\\') {
                escape = true;
                continue;
            }
            if (c == '"') {
                inString = !inString;
                continue;
            }
            if (inString) continue;

            if (c == '{' || c == '[') {
                stack.push_back(c);
            } else if (c == '}' || c == ']') {
                if (!stack.empty()) {
                    char open = stack.back();
                    if ((open == '{' && c == '}') || (open == '[' && c == ']')) {
                        stack.pop_back();
                        if (stack.empty()) {
                            return body.substr(pos, i - pos + 1);
                        }
                    }
                }
            }
        }
        return "";
    }

    size_t end = pos;
    while (end < body.size() && body[end] != ',' && body[end] != '}' && !std::isspace(static_cast<unsigned char>(body[end]))) {
        end++;
    }
    return body.substr(pos, end - pos);
}

// Helper: Extract bot token and method from path like "/bot<token>/method_name?params"
bool extractBotApiPath(const std::string& path, std::string& bot_token, std::string& method_name) {
    // Format: /bot<token>/method_name or /bot<token>/method_name?param=value
    if (path.length() < 5 || path.substr(0, 4) != "/bot") {
        Logger::getInstance().warning("Bot API path doesn't start with /bot: " + path);
        return false;
    }
    
    size_t token_start = 4; // After "/bot"
    
    // Find query string start first
    size_t query_start = path.find('?', token_start);
    
    // Find method start (first '/' after token_start)
    size_t method_start = path.find('/', token_start);
    
    if (method_start == std::string::npos) {
        // Try to find method name without slash (e.g., /bot<token>method_name)
        // This shouldn't happen, but let's be defensive
        Logger::getInstance().warning("Bot API path missing method separator: " + path);
        return false;
    }
    
    // Extract token (between /bot and /method_name)
    bot_token = path.substr(token_start, method_start - token_start);
    if (bot_token.empty()) {
        Logger::getInstance().warning("Bot API path has empty token: " + path);
        return false;
    }
    
    // Extract method name (after /method_name, before ? if query string exists)
    size_t method_end = (query_start != std::string::npos) ? query_start : path.length();
    if (method_start + 1 >= method_end) {
        Logger::getInstance().warning("Bot API path has empty method name: " + path);
        return false;
    }
    
    method_name = path.substr(method_start + 1, method_end - method_start - 1);
    
    // Remove query string from method_name if present (shouldn't happen, but be safe)
    size_t qm_pos = method_name.find('?');
    if (qm_pos != std::string::npos) {
        method_name = method_name.substr(0, qm_pos);
    }
    
    // Convert to lowercase for case-insensitive matching
    std::transform(method_name.begin(), method_name.end(), method_name.begin(), ::tolower);
    
    if (method_name.empty()) {
        Logger::getInstance().warning("Bot API path has empty method name after processing: " + path);
        return false;
    }
    
    return true;
}

// Helper: Create Telegram Bot API response format
std::string createBotApiResponse(bool ok, const std::string& result = "", int error_code = 0, const std::string& description = "") {
    std::ostringstream oss;
    oss << "{\"ok\":" << (ok ? "true" : "false");
    
    if (ok && !result.empty()) {
        oss << ",\"result\":" << result;
    }
    
    if (!ok) {
        if (error_code > 0) {
            oss << ",\"error_code\":" << error_code;
        }
        if (!description.empty()) {
            oss << ",\"description\":\"" << JsonParser::escapeJson(description) << "\"";
        }
    }
    
    oss << "}";
    return oss.str();
}

// Handle Bot API requests
std::string RequestHandler::handleBotApiRequest(const std::string& path, const std::string& method,
                                                const std::string& body, const std::map<std::string, std::string>& /*headers*/) {
    std::string bot_token, method_name;
    
    Logger::getInstance().info("Bot API request path: " + path + " method: " + method);
    
    if (!extractBotApiPath(path, bot_token, method_name)) {
        Logger::getInstance().warning("Failed to parse Bot API path: " + path);
        return createBotApiResponse(false, "", 400, "Invalid Bot API path format. Use /bot<token>/method_name");
    }
    
    Logger::getInstance().info("Bot API parsed: token=" + bot_token.substr(0, std::min(8UL, bot_token.length())) + "... method=" + method_name);
    
    // Extract query string parameters for GET requests
    std::map<std::string, std::string> query_params;
    size_t query_start = path.find('?');
    if (query_start != std::string::npos && method == "GET") {
        std::string query_string = path.substr(query_start + 1);
        query_params = parseQueryString(query_string);
    }
    
    // Validate bot token
    auto& storage = InMemoryStorage::getInstance();
    auto bot = storage.getBotByToken(bot_token);
    
    if (bot.id.empty()) {
        if (method_name != "sendmessage") {
            return createBotApiResponse(false, "", 401, "Unauthorized");
        }
    } else if (!bot.is_active) {
        return createBotApiResponse(false, "", 403, "Bot is not active");
    }
    
    // Route to appropriate handler
    if (method_name == "getme") {
        return handleBotApiGetMe(bot);
    } else if (method_name == "getupdates") {
        return handleBotApiGetUpdates(bot_token, body, query_params);
    } else if (method_name == "setwebhook") {
        return handleBotApiSetWebhook(bot_token, body);
    } else if (method_name == "deletewebhook") {
        return handleBotApiDeleteWebhook(bot_token, body);
    } else if (method_name == "getwebhookinfo") {
        return handleBotApiGetWebhookInfo(bot_token);
    } else if (method_name == "sendmessage") {
        return handleBotApiSendMessage(bot_token, body);
    } else if (method_name == "editmessagetext") {
        return handleBotApiEditMessageText(bot_token, body);
    } else if (method_name == "deletemessage") {
        return handleBotApiDeleteMessage(bot_token, body);
    } else if (method_name == "forwardmessage") {
        return handleBotApiForwardMessage(bot_token, body);
    } else if (method_name == "copymessage") {
        return handleBotApiCopyMessage(bot_token, body);
    } else if (method_name == "sendphoto") {
        return handleBotApiSendPhoto(bot_token, body);
    } else if (method_name == "sendaudio") {
        return handleBotApiSendAudio(bot_token, body);
    } else if (method_name == "sendvideo") {
        return handleBotApiSendVideo(bot_token, body);
    } else if (method_name == "senddocument") {
        return handleBotApiSendDocument(bot_token, body);
    } else if (method_name == "sendvoice") {
        return handleBotApiSendVoice(bot_token, body);
    } else {
        return createBotApiResponse(false, "", 404, "Method not found: " + method_name);
    }
}

// Bot API: getMe
std::string RequestHandler::handleBotApiGetMe(const InMemoryStorage::Bot& bot) {
    std::ostringstream oss;
    oss << "{"
        << "\"id\":\"" << JsonParser::escapeJson(bot.id) << "\","
        << "\"is_bot\":true,"
        << "\"first_name\":\"" << JsonParser::escapeJson(bot.first_name) << "\","
        << "\"username\":\"" << JsonParser::escapeJson(bot.username) << "\","
        << "\"can_join_groups\":true,"
        << "\"can_read_all_group_messages\":true,"
        << "\"supports_inline_queries\":true"
        << "}";
    
    return createBotApiResponse(true, oss.str());
}

// Bot API: getUpdates
std::string RequestHandler::handleBotApiGetUpdates(const std::string& bot_token, const std::string& body,
                                                   const std::map<std::string, std::string>& query_params) {
    // Parse parameters from body (POST) or query string (GET)
    std::map<std::string, std::string> data;
    
    if (!body.empty()) {
        data = JsonParser::parse(body);
    }
    
    // Merge query params (GET takes precedence)
    for (const auto& pair : query_params) {
        data[pair.first] = pair.second;
    }
    
    int64_t offset = 0;
    int limit = 100;
    int timeout = 0;
    
    if (data.find("offset") != data.end()) {
        try {
            offset = std::stoll(data["offset"]);
        } catch (...) {}
    }
    
    if (data.find("limit") != data.end()) {
        try {
            limit = std::stoi(data["limit"]);
            if (limit < 1) limit = 1;
            if (limit > 100) limit = 100;
        } catch (...) {}
    }
    
    if (data.find("timeout") != data.end()) {
        try {
            timeout = std::stoi(data["timeout"]);
        } catch (...) {}
    }
    
    auto& storage = InMemoryStorage::getInstance();
    auto updates = storage.getUpdates(bot_token, offset, limit, timeout);
    
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto& update : updates) {
        if (!first) oss << ",";
        first = false;
        
        oss << "{"
            << "\"update_id\":" << update.update_id << ","
            << update.update_data  // Already JSON string
            << "}";
    }
    oss << "]";
    
    return createBotApiResponse(true, oss.str());
}

// Bot API: setWebhook
std::string RequestHandler::handleBotApiSetWebhook(const std::string& bot_token, const std::string& body) {
    auto data = JsonParser::parse(body);
    
    if (data.find("url") == data.end()) {
        return createBotApiResponse(false, "", 400, "url parameter is required");
    }
    
    std::string url = data["url"];
    std::string secret_token = (data.find("secret_token") != data.end()) ? data["secret_token"] : "";
    int max_connections = 40;
    std::vector<std::string> allowed_updates;
    
    if (data.find("max_connections") != data.end()) {
        try {
            max_connections = std::stoi(data["max_connections"]);
            if (max_connections < 1) max_connections = 1;
            if (max_connections > 100) max_connections = 100;
        } catch (...) {}
    }
    
    if (data.find("allowed_updates") != data.end()) {
        // Parse array - simplified, assumes JSON array format
        std::string allowed = data["allowed_updates"];
        // TODO: Parse JSON array properly
    }
    
    auto& storage = InMemoryStorage::getInstance();
    
    if (storage.setWebhook(bot_token, url, secret_token, max_connections, allowed_updates)) {
        return createBotApiResponse(true, "true");
    } else {
        return createBotApiResponse(false, "", 500, "Failed to set webhook");
    }
}

// Bot API: deleteWebhook
std::string RequestHandler::handleBotApiDeleteWebhook(const std::string& bot_token, const std::string& body) {
    auto data = JsonParser::parse(body);
    
    bool drop_pending_updates = false;
    if (data.find("drop_pending_updates") != data.end()) {
        drop_pending_updates = (data["drop_pending_updates"] == "true");
    }
    
    auto& storage = InMemoryStorage::getInstance();
    
    if (storage.deleteWebhook(bot_token, drop_pending_updates)) {
        return createBotApiResponse(true, "true");
    } else {
        return createBotApiResponse(false, "", 500, "Failed to delete webhook");
    }
}

// Bot API: getWebhookInfo
std::string RequestHandler::handleBotApiGetWebhookInfo(const std::string& bot_token) {
    auto& storage = InMemoryStorage::getInstance();
    auto info = storage.getWebhookInfo(bot_token);
    
    std::ostringstream oss;
    oss << "{"
        << "\"url\":\"" << JsonParser::escapeJson(info.url) << "\","
        << "\"has_custom_certificate\":" << (info.has_custom_certificate ? "true" : "false") << ","
        << "\"pending_update_count\":" << info.pending_update_count << ","
        << "\"max_connections\":" << info.max_connections;
    
    if (!info.last_error_date.empty()) {
        oss << ",\"last_error_date\":" << info.last_error_date;
    }
    
    if (!info.last_error_message.empty()) {
        oss << ",\"last_error_message\":\"" << JsonParser::escapeJson(info.last_error_message) << "\"";
    }
    
    oss << "}";
    
    return createBotApiResponse(true, oss.str());
}

// Bot API: sendMessage
std::string RequestHandler::handleBotApiSendMessage(const std::string& bot_token, const std::string& body) {
    auto data = JsonParser::parse(body);
    
    if (data.find("chat_id") == data.end() || data.find("text") == data.end()) {
        return createBotApiResponse(false, "", 400, "chat_id and text are required");
    }
    
    std::string chat_id = data["chat_id"];
    std::string text = data["text"];
    
    // Parse reply_markup (inline or reply keyboard)
    std::string reply_markup = extractJsonValueForKey(body, "reply_markup");
    if (reply_markup.empty() && data.find("reply_markup") != data.end()) {
        std::string reply_markup_value = data["reply_markup"];
        if (!reply_markup_value.empty()) {
            reply_markup = reply_markup_value;
        }
    }
    reply_markup = trimString(reply_markup);
    if (!reply_markup.empty()) {
        if (reply_markup[0] == '[') {
            reply_markup = "{\"inline_keyboard\":" + reply_markup + "}";
        } else if (reply_markup[0] != '{') {
            reply_markup.clear();
        }
    }
    
    // Get bot info from database
    auto builder_bot = db_manager_.getBotBuilderBotByToken(bot_token);
    if (!builder_bot.id.empty() && !builder_bot.bot_user_id.empty()) {
        if (!builder_bot.is_active) {
            return createBotApiResponse(false, "", 403, "Bot is not active");
        }
        
        // Determine chat type and send message
        // For now, assume it's a private chat (user_id)
        std::string sender_id = builder_bot.bot_user_id; // Bot user ID as sender
        std::string receiver_id = chat_id;
        
        // Send message using database manager
        if (db_manager_.sendMessage(sender_id, receiver_id, text, "text", "", "", 0, "", "", "", "", reply_markup)) {
            // Get the last message to return its ID
            Message lastMessage = db_manager_.getLastMessage(sender_id, receiver_id);
            
            // Get bot user info for response
            auto bot_user = db_manager_.getUserById(builder_bot.bot_user_id);
            
            // Return Message object in Telegram Bot API format
            std::ostringstream oss;
            oss << "{"
                << "\"message_id\":\"" << JsonParser::escapeJson(lastMessage.id) << "\","
                << "\"from\":{"
                << "\"id\":\"" << JsonParser::escapeJson(builder_bot.bot_user_id) << "\","
                << "\"is_bot\":true,"
                << "\"first_name\":\"" << JsonParser::escapeJson(builder_bot.bot_name) << "\","
                << "\"username\":\"" << JsonParser::escapeJson(bot_user.username) << "\""
                << "},"
                << "\"chat\":{"
                << "\"id\":\"" << JsonParser::escapeJson(receiver_id) << "\","
                << "\"type\":\"private\""
                << "},"
                << "\"date\":" << (time(nullptr)) << ","
                << "\"text\":\"" << JsonParser::escapeJson(text) << "\"";
            
            // Add reply_markup if present
            if (!reply_markup.empty()) {
                oss << ",\"reply_markup\":" << reply_markup;
            }
            
            oss << "}";
            
            return createBotApiResponse(true, oss.str());
        } else {
            return createBotApiResponse(false, "", 500, "Failed to send message");
        }
    }
    
    auto& storage = InMemoryStorage::getInstance();
    auto bot = storage.getBotByToken(bot_token);
    if (bot.id.empty()) {
        return createBotApiResponse(false, "", 401, "Unauthorized");
    }
    if (!bot.is_active) {
        return createBotApiResponse(false, "", 403, "Bot is not active");
    }
    
    std::string sender_id = bot.id;
    std::string receiver_id = chat_id;
    std::string message_id;
    
    if (storage.sendMessage(sender_id, receiver_id, text, "text", "", "", 0, "", "", "", "", &message_id)) {
        std::ostringstream oss;
        oss << "{"
            << "\"message_id\":\"" << JsonParser::escapeJson(message_id) << "\","
            << "\"from\":{"
            << "\"id\":\"" << JsonParser::escapeJson(sender_id) << "\","
            << "\"is_bot\":true,"
            << "\"first_name\":\"" << JsonParser::escapeJson(bot.first_name) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(bot.username) << "\""
            << "},"
            << "\"chat\":{"
            << "\"id\":\"" << JsonParser::escapeJson(receiver_id) << "\","
            << "\"type\":\"private\""
            << "},"
            << "\"date\":" << storage.getCurrentTimestampInt() << ","
            << "\"text\":\"" << JsonParser::escapeJson(text) << "\"";
        
        if (!reply_markup.empty()) {
            oss << ",\"reply_markup\":" << reply_markup;
        }
        
        oss << "}";
        
        return createBotApiResponse(true, oss.str());
    }
    
    return createBotApiResponse(false, "", 500, "Failed to send message");
}

// Bot API: editMessageText
std::string RequestHandler::handleBotApiEditMessageText(const std::string& bot_token, const std::string& body) {
    auto data = JsonParser::parse(body);
    
    std::string message_id;
    std::string chat_id;
    std::string text;
    
    // Parse required parameters
    if (data.find("message_id") != data.end()) {
        message_id = data["message_id"];
    }
    if (data.find("chat_id") != data.end()) {
        chat_id = data["chat_id"];
    }
    if (data.find("text") != data.end()) {
        text = data["text"];
    }
    
    if (message_id.empty() || text.empty()) {
        return createBotApiResponse(false, "", 400, "message_id and text are required");
    }
    
    auto& storage = InMemoryStorage::getInstance();
    auto bot = storage.getBotByToken(bot_token);
    
    if (bot.id.empty()) {
        return createBotApiResponse(false, "", 401, "Unauthorized");
    }
    
    // Get message and verify ownership
    auto msg = storage.getMessageById(message_id);
    if (msg.id.empty() || msg.sender_id != bot.id) {
        return createBotApiResponse(false, "", 400, "Message not found or not owned by bot");
    }
    
    // Edit message
    if (storage.editMessage(message_id, text)) {
        // Return edited message
        std::ostringstream oss;
        oss << "{"
            << "\"message_id\":\"" << JsonParser::escapeJson(message_id) << "\","
            << "\"from\":{"
            << "\"id\":\"" << JsonParser::escapeJson(bot.id) << "\","
            << "\"is_bot\":true,"
            << "\"first_name\":\"" << JsonParser::escapeJson(bot.first_name) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(bot.username) << "\""
            << "},"
            << "\"chat\":{"
            << "\"id\":\"" << JsonParser::escapeJson(msg.receiver_id) << "\","
            << "\"type\":\"private\""
            << "},"
            << "\"date\":" << storage.getCurrentTimestampInt() << ","
            << "\"edit_date\":" << storage.getCurrentTimestampInt() << ","
            << "\"text\":\"" << JsonParser::escapeJson(text) << "\""
            << "}";
        
        return createBotApiResponse(true, oss.str());
    } else {
        return createBotApiResponse(false, "", 500, "Failed to edit message");
    }
}

// Bot API: deleteMessage
std::string RequestHandler::handleBotApiDeleteMessage(const std::string& bot_token, const std::string& body) {
    auto data = JsonParser::parse(body);
    
    std::string message_id;
    std::string chat_id;
    
    if (data.find("message_id") != data.end()) {
        message_id = data["message_id"];
    }
    if (data.find("chat_id") != data.end()) {
        chat_id = data["chat_id"];
    }
    
    if (message_id.empty()) {
        return createBotApiResponse(false, "", 400, "message_id is required");
    }
    
    auto& storage = InMemoryStorage::getInstance();
    auto bot = storage.getBotByToken(bot_token);
    
    if (bot.id.empty()) {
        return createBotApiResponse(false, "", 401, "Unauthorized");
    }
    
    // Get message and verify ownership
    auto msg = storage.getMessageById(message_id);
    if (msg.id.empty() || msg.sender_id != bot.id) {
        return createBotApiResponse(false, "", 400, "Message not found or not owned by bot");
    }
    
    // Delete message
    if (storage.deleteMessage(message_id)) {
        return createBotApiResponse(true, "true");
    } else {
        return createBotApiResponse(false, "", 500, "Failed to delete message");
    }
}

// Bot API: forwardMessage
std::string RequestHandler::handleBotApiForwardMessage(const std::string& bot_token, const std::string& body) {
    auto data = JsonParser::parse(body);
    
    std::string chat_id = data.find("chat_id") != data.end() ? data["chat_id"] : "";
    std::string from_chat_id = data.find("from_chat_id") != data.end() ? data["from_chat_id"] : "";
    std::string message_id_str = data.find("message_id") != data.end() ? data["message_id"] : "";
    
    if (chat_id.empty() || from_chat_id.empty() || message_id_str.empty()) {
        return createBotApiResponse(false, "", 400, "chat_id, from_chat_id, and message_id are required");
    }
    
    auto& storage = InMemoryStorage::getInstance();
    auto bot = storage.getBotByToken(bot_token);
    
    if (bot.id.empty()) {
        return createBotApiResponse(false, "", 401, "Unauthorized");
    }
    
    // Get original message
    auto original_msg = storage.getMessageById(message_id_str);
    if (original_msg.id.empty()) {
        return createBotApiResponse(false, "", 400, "Original message not found");
    }
    
    // Forward message (send with forwarded info)
    std::string new_message_id;
    bool success = storage.sendMessage(
        bot.id,  // sender (bot)
        chat_id,  // receiver
        original_msg.content,
        original_msg.message_type,
        original_msg.file_path,
        original_msg.file_name,
        original_msg.file_size,
        "",  // reply_to_message_id
        original_msg.sender_id,  // forwarded_from_user_id
        storage.getUserById(original_msg.sender_id).username,  // forwarded_from_username
        original_msg.id,  // forwarded_from_message_id
        &new_message_id
    );
    
    if (success) {
        std::ostringstream oss;
        oss << "{"
            << "\"message_id\":\"" << JsonParser::escapeJson(new_message_id) << "\","
            << "\"from\":{"
            << "\"id\":\"" << JsonParser::escapeJson(bot.id) << "\","
            << "\"is_bot\":true,"
            << "\"first_name\":\"" << JsonParser::escapeJson(bot.first_name) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(bot.username) << "\""
            << "},"
            << "\"chat\":{"
            << "\"id\":\"" << JsonParser::escapeJson(chat_id) << "\","
            << "\"type\":\"private\""
            << "},"
            << "\"date\":" << storage.getCurrentTimestampInt() << ","
            << "\"forward_from\":{"
            << "\"id\":\"" << JsonParser::escapeJson(original_msg.sender_id) << "\""
            << "},"
            << "\"forward_from_message_id\":" << original_msg.id << ","
            << "\"text\":\"" << JsonParser::escapeJson(original_msg.content) << "\""
            << "}";
        
        return createBotApiResponse(true, oss.str());
    } else {
        return createBotApiResponse(false, "", 500, "Failed to forward message");
    }
}

// Bot API: copyMessage
std::string RequestHandler::handleBotApiCopyMessage(const std::string& bot_token, const std::string& body) {
    auto data = JsonParser::parse(body);
    
    std::string chat_id = data.find("chat_id") != data.end() ? data["chat_id"] : "";
    std::string from_chat_id = data.find("from_chat_id") != data.end() ? data["from_chat_id"] : "";
    std::string message_id_str = data.find("message_id") != data.end() ? data["message_id"] : "";
    
    if (chat_id.empty() || from_chat_id.empty() || message_id_str.empty()) {
        return createBotApiResponse(false, "", 400, "chat_id, from_chat_id, and message_id are required");
    }
    
    auto& storage = InMemoryStorage::getInstance();
    auto bot = storage.getBotByToken(bot_token);
    
    if (bot.id.empty()) {
        return createBotApiResponse(false, "", 401, "Unauthorized");
    }
    
    // Get original message
    auto original_msg = storage.getMessageById(message_id_str);
    if (original_msg.id.empty()) {
        return createBotApiResponse(false, "", 400, "Original message not found");
    }
    
    // Copy message (send without forwarded info)
    std::string new_message_id;
    bool success = storage.sendMessage(
        bot.id,  // sender (bot)
        chat_id,  // receiver
        original_msg.content,
        original_msg.message_type,
        original_msg.file_path,
        original_msg.file_name,
        original_msg.file_size,
        "",  // reply_to_message_id
        "",  // not forwarded
        "",
        "",
        &new_message_id
    );
    
    if (success) {
        std::ostringstream oss;
        oss << "{"
            << "\"message_id\":\"" << JsonParser::escapeJson(new_message_id) << "\","
            << "\"from\":{"
            << "\"id\":\"" << JsonParser::escapeJson(bot.id) << "\","
            << "\"is_bot\":true,"
            << "\"first_name\":\"" << JsonParser::escapeJson(bot.first_name) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(bot.username) << "\""
            << "},"
            << "\"chat\":{"
            << "\"id\":\"" << JsonParser::escapeJson(chat_id) << "\","
            << "\"type\":\"private\""
            << "},"
            << "\"date\":" << storage.getCurrentTimestampInt() << ","
            << "\"text\":\"" << JsonParser::escapeJson(original_msg.content) << "\""
            << "}";
        
        return createBotApiResponse(true, oss.str());
    } else {
        return createBotApiResponse(false, "", 500, "Failed to copy message");
    }
}

// Bot API: sendPhoto
std::string RequestHandler::handleBotApiSendPhoto(const std::string& bot_token, const std::string& body) {
    auto data = JsonParser::parse(body);
    
    if (data.find("chat_id") == data.end() || data.find("photo") == data.end()) {
        return createBotApiResponse(false, "", 400, "chat_id and photo are required");
    }
    
    std::string chat_id = data["chat_id"];
    std::string photo = data["photo"];
    std::string caption = data.find("caption") != data.end() ? data["caption"] : "";
    
    auto& storage = InMemoryStorage::getInstance();
    auto bot = storage.getBotByToken(bot_token);
    
    if (bot.id.empty()) {
        return createBotApiResponse(false, "", 401, "Unauthorized");
    }
    
    std::string sender_id = bot.id;
    std::string receiver_id = chat_id;
    std::string message_id;
    
    if (storage.sendMessage(sender_id, receiver_id, caption.empty() ? "[Photo]" : caption, 
                           "file", photo, "photo.jpg", 0, "", "", "", "", &message_id)) {
        std::ostringstream oss;
        oss << "{\"message_id\":\"" << JsonParser::escapeJson(message_id) << "\","
            << "\"from\":{\"id\":\"" << JsonParser::escapeJson(bot.id) << "\",\"is_bot\":true,"
            << "\"first_name\":\"" << JsonParser::escapeJson(bot.first_name) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(bot.username) << "\"},"
            << "\"chat\":{\"id\":\"" << JsonParser::escapeJson(receiver_id) << "\",\"type\":\"private\"},"
            << "\"date\":" << storage.getCurrentTimestampInt() << ","
            << "\"photo\":[{\"file_id\":\"" << JsonParser::escapeJson(photo) << "\"}],"
            << "\"caption\":\"" << JsonParser::escapeJson(caption) << "\"}";
        
        return createBotApiResponse(true, oss.str());
    } else {
        return createBotApiResponse(false, "", 500, "Failed to send photo");
    }
}

// Bot API: sendAudio
std::string RequestHandler::handleBotApiSendAudio(const std::string& bot_token, const std::string& body) {
    auto data = JsonParser::parse(body);
    
    if (data.find("chat_id") == data.end() || data.find("audio") == data.end()) {
        return createBotApiResponse(false, "", 400, "chat_id and audio are required");
    }
    
    std::string chat_id = data["chat_id"];
    std::string audio = data["audio"];
    std::string caption = data.find("caption") != data.end() ? data["caption"] : "";
    
    auto& storage = InMemoryStorage::getInstance();
    auto bot = storage.getBotByToken(bot_token);
    
    if (bot.id.empty()) {
        return createBotApiResponse(false, "", 401, "Unauthorized");
    }
    
    std::string sender_id = bot.id;
    std::string receiver_id = chat_id;
    std::string message_id;
    
    if (storage.sendMessage(sender_id, receiver_id, caption.empty() ? "[Audio]" : caption, 
                           "file", audio, "audio.mp3", 0, "", "", "", "", &message_id)) {
        std::ostringstream oss;
        oss << "{\"message_id\":\"" << JsonParser::escapeJson(message_id) << "\","
            << "\"from\":{\"id\":\"" << JsonParser::escapeJson(bot.id) << "\",\"is_bot\":true,"
            << "\"first_name\":\"" << JsonParser::escapeJson(bot.first_name) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(bot.username) << "\"},"
            << "\"chat\":{\"id\":\"" << JsonParser::escapeJson(receiver_id) << "\",\"type\":\"private\"},"
            << "\"date\":" << storage.getCurrentTimestampInt() << ","
            << "\"audio\":{\"file_id\":\"" << JsonParser::escapeJson(audio) << "\"},"
            << "\"caption\":\"" << JsonParser::escapeJson(caption) << "\"}";
        
        return createBotApiResponse(true, oss.str());
    } else {
        return createBotApiResponse(false, "", 500, "Failed to send audio");
    }
}

// Bot API: sendVideo
std::string RequestHandler::handleBotApiSendVideo(const std::string& bot_token, const std::string& body) {
    auto data = JsonParser::parse(body);
    
    if (data.find("chat_id") == data.end() || data.find("video") == data.end()) {
        return createBotApiResponse(false, "", 400, "chat_id and video are required");
    }
    
    std::string chat_id = data["chat_id"];
    std::string video = data["video"];
    std::string caption = data.find("caption") != data.end() ? data["caption"] : "";
    
    auto& storage = InMemoryStorage::getInstance();
    auto bot = storage.getBotByToken(bot_token);
    
    if (bot.id.empty()) {
        return createBotApiResponse(false, "", 401, "Unauthorized");
    }
    
    std::string sender_id = bot.id;
    std::string receiver_id = chat_id;
    std::string message_id;
    
    if (storage.sendMessage(sender_id, receiver_id, caption.empty() ? "[Video]" : caption, 
                           "file", video, "video.mp4", 0, "", "", "", "", &message_id)) {
        std::ostringstream oss;
        oss << "{\"message_id\":\"" << JsonParser::escapeJson(message_id) << "\","
            << "\"from\":{\"id\":\"" << JsonParser::escapeJson(bot.id) << "\",\"is_bot\":true,"
            << "\"first_name\":\"" << JsonParser::escapeJson(bot.first_name) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(bot.username) << "\"},"
            << "\"chat\":{\"id\":\"" << JsonParser::escapeJson(receiver_id) << "\",\"type\":\"private\"},"
            << "\"date\":" << storage.getCurrentTimestampInt() << ","
            << "\"video\":{\"file_id\":\"" << JsonParser::escapeJson(video) << "\"},"
            << "\"caption\":\"" << JsonParser::escapeJson(caption) << "\"}";
        
        return createBotApiResponse(true, oss.str());
    } else {
        return createBotApiResponse(false, "", 500, "Failed to send video");
    }
}

// Bot API: sendDocument
std::string RequestHandler::handleBotApiSendDocument(const std::string& bot_token, const std::string& body) {
    auto data = JsonParser::parse(body);
    
    if (data.find("chat_id") == data.end() || data.find("document") == data.end()) {
        return createBotApiResponse(false, "", 400, "chat_id and document are required");
    }
    
    std::string chat_id = data["chat_id"];
    std::string document = data["document"];
    std::string caption = data.find("caption") != data.end() ? data["caption"] : "";
    
    auto& storage = InMemoryStorage::getInstance();
    auto bot = storage.getBotByToken(bot_token);
    
    if (bot.id.empty()) {
        return createBotApiResponse(false, "", 401, "Unauthorized");
    }
    
    std::string sender_id = bot.id;
    std::string receiver_id = chat_id;
    std::string message_id;
    
    if (storage.sendMessage(sender_id, receiver_id, caption.empty() ? "[Document]" : caption, 
                           "file", document, "document", 0, "", "", "", "", &message_id)) {
        std::ostringstream oss;
        oss << "{\"message_id\":\"" << JsonParser::escapeJson(message_id) << "\","
            << "\"from\":{\"id\":\"" << JsonParser::escapeJson(bot.id) << "\",\"is_bot\":true,"
            << "\"first_name\":\"" << JsonParser::escapeJson(bot.first_name) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(bot.username) << "\"},"
            << "\"chat\":{\"id\":\"" << JsonParser::escapeJson(receiver_id) << "\",\"type\":\"private\"},"
            << "\"date\":" << storage.getCurrentTimestampInt() << ","
            << "\"document\":{\"file_id\":\"" << JsonParser::escapeJson(document) << "\"},"
            << "\"caption\":\"" << JsonParser::escapeJson(caption) << "\"}";
        
        return createBotApiResponse(true, oss.str());
    } else {
        return createBotApiResponse(false, "", 500, "Failed to send document");
    }
}

// Bot API: sendVoice
std::string RequestHandler::handleBotApiSendVoice(const std::string& bot_token, const std::string& body) {
    auto data = JsonParser::parse(body);
    
    if (data.find("chat_id") == data.end() || data.find("voice") == data.end()) {
        return createBotApiResponse(false, "", 400, "chat_id and voice are required");
    }
    
    std::string chat_id = data["chat_id"];
    std::string voice = data["voice"];
    std::string caption = data.find("caption") != data.end() ? data["caption"] : "";
    
    auto& storage = InMemoryStorage::getInstance();
    auto bot = storage.getBotByToken(bot_token);
    
    if (bot.id.empty()) {
        return createBotApiResponse(false, "", 401, "Unauthorized");
    }
    
    std::string sender_id = bot.id;
    std::string receiver_id = chat_id;
    std::string message_id;
    
    if (storage.sendMessage(sender_id, receiver_id, caption.empty() ? "[Voice]" : caption, 
                           "voice", voice, "voice.ogg", 0, "", "", "", "", &message_id)) {
        std::ostringstream oss;
        oss << "{\"message_id\":\"" << JsonParser::escapeJson(message_id) << "\","
            << "\"from\":{\"id\":\"" << JsonParser::escapeJson(bot.id) << "\",\"is_bot\":true,"
            << "\"first_name\":\"" << JsonParser::escapeJson(bot.first_name) << "\","
            << "\"username\":\"" << JsonParser::escapeJson(bot.username) << "\"},"
            << "\"chat\":{\"id\":\"" << JsonParser::escapeJson(receiver_id) << "\",\"type\":\"private\"},"
            << "\"date\":" << storage.getCurrentTimestampInt() << ","
            << "\"voice\":{\"file_id\":\"" << JsonParser::escapeJson(voice) << "\"},"
            << "\"caption\":\"" << JsonParser::escapeJson(caption) << "\"}";
        
        return createBotApiResponse(true, oss.str());
    } else {
        return createBotApiResponse(false, "", 500, "Failed to send voice");
    }
}

} // namespace xipher
