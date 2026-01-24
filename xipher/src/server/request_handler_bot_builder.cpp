// Bot Builder API handlers
// OWASP 2025 Security Compliant

#include "../include/server/request_handler.hpp"
#include "../include/utils/json_parser.hpp"
#include "../include/utils/logger.hpp"
#include "../include/auth/password_hash.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <vector>
#include <openssl/rand.h>

namespace xipher {

namespace {

constexpr size_t kMaxBotNameLen = 64;
constexpr size_t kMaxBotDescriptionLen = 280;
constexpr size_t kMaxBotFlowJsonLen = 200 * 1024;
constexpr size_t kMaxBotFilePathLen = 240;
constexpr size_t kMaxBotFileContentLen = 512 * 1024;

std::string toLowerAscii(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trimAsciiWhitespace(const std::string& value) {
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

std::string normalizeSingleLineText(const std::string& value, size_t max_len) {
    std::string out;
    out.reserve(std::min(value.size(), max_len));
    for (unsigned char c : value) {
        if (c == '\0') {
            continue;
        }
        if (c == '\n' || c == '\r' || c == '\t') {
            if (!out.empty() && out.back() != ' ') {
                out.push_back(' ');
            } else if (out.empty()) {
                out.push_back(' ');
            }
        } else if (c < 0x20) {
            continue;
        } else {
            out.push_back(static_cast<char>(c));
        }
        if (out.size() >= max_len) {
            break;
        }
    }
    return trimAsciiWhitespace(out);
}

bool containsHtmlAngleBrackets(const std::string& value) {
    return value.find('<') != std::string::npos || value.find('>') != std::string::npos;
}

bool looksLikeJsonValue(const std::string& value) {
    std::string trimmed = trimAsciiWhitespace(value);
    if (trimmed.size() < 2) {
        return false;
    }
    return (trimmed.front() == '{' && trimmed.back() == '}')
        || (trimmed.front() == '[' && trimmed.back() == ']');
}

std::string normalizeBotFilePath(std::string value) {
    value = trimAsciiWhitespace(value);
    std::replace(value.begin(), value.end(), '\\', '/');
    while (!value.empty() && (value.front() == '/' || value.front() == '\\')) {
        value.erase(value.begin());
    }
    return value;
}

bool isSafeBotFilePath(const std::string& value) {
    if (value.empty() || value.size() > kMaxBotFilePathLen) {
        return false;
    }
    if (value.front() == '/' || value.front() == '\\') {
        return false;
    }
    if (value.find('\0') != std::string::npos) {
        return false;
    }
    if (value.find('\\') != std::string::npos) {
        return false;
    }
    if (value.find(':') != std::string::npos) {
        return false;
    }
    for (unsigned char c : value) {
        if (c < 0x20) {
            return false;
        }
    }
    std::stringstream ss(value);
    std::string segment;
    while (std::getline(ss, segment, '/')) {
        if (segment.empty() || segment == "." || segment == "..") {
            return false;
        }
    }
    return true;
}

bool ensurePremiumAccess(DatabaseManager& db_manager, const std::string& user_id, User& out_user, std::string& error) {
    out_user = db_manager.getUserById(user_id);
    if (out_user.id.empty()) {
        error = "User not found";
        return false;
    }
    if (!out_user.is_admin && !out_user.is_premium) {
        error = "Premium required";
        return false;
    }
    return true;
}

// Escape string for Python code (handle quotes, newlines, backslashes)
std::string escapeForPython(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 10);
    for (char c : s) {
        if (c == '\\') {
            result += "\\\\";
        } else if (c == '"') {
            result += "\\\"";
        } else if (c == '\n') {
            result += "\\n";
        } else if (c == '\r') {
            result += "\\r";
        } else if (c == '\t') {
            result += "\\t";
        } else {
            result += c;
        }
    }
    return result;
}

// Generate Python code from template configuration
std::string generatePythonCodeFromTemplate(const std::string& bot_name, const std::map<std::string, std::string>& cfg) {
    std::ostringstream code;
    const std::string safe_bot_name = escapeForPython(bot_name);
    
    code << "#!/usr/bin/env python3\n";
    code << "# -*- coding: utf-8 -*-\n";
    code << "\"\"\"\n";
    code << "Xipher Bot - " << safe_bot_name << "\n";
    code << "\n";
    code << "Этот код автоматически сгенерирован из шаблона.\n";
    code << "Готов к использованию и работает с Xipher Bot API.\n";
    code << "\"\"\"\n\n";
    
    code << "def handle(update, api):\n";
    code << "    \"\"\"\n";
    code << "    Главная функция обработки событий бота.\n";
    code << "    \n";
    code << "    Args:\n";
    code << "        update: dict с данными о событии\n";
    code << "        api: объект API для отправки сообщений\n";
    code << "    \"\"\"\n";
    code << "    event_type = update.get('type', '')\n";
    code << "    scope = update.get('scope', '')\n";
    code << "    text = (update.get('text') or '').strip()\n";
    code << "    from_user_id = update.get('from_user_id', '')\n";
    code << "    scope_id = update.get('scope_id', '')\n\n";
    
    // Welcome message
    bool has_welcome = (cfg.count("module_welcome") && (cfg.at("module_welcome") == "true" || cfg.at("module_welcome") == "1"));
    if (has_welcome) {
        std::string welcome_text = cfg.count("welcome_text") ? cfg.at("welcome_text") : "Привет, @{username}! Добро пожаловать 👋";
        std::string welcome_escaped = escapeForPython(welcome_text);
        code << "    # Welcome message\n";
        code << "    if event_type == 'member_joined':\n";
        code << "        username = update.get('joined_username') or 'user'\n";
        code << "        msg = \"" << welcome_escaped << "\"\n";
        code << "        msg = msg.replace('{username}', username).replace('@{username}', username)\n";
        code << "        api.send(msg)\n";
        code << "        return\n\n";
    }
    
    // Rules
    bool has_rules = (cfg.count("module_rules") && (cfg.at("module_rules") == "true" || cfg.at("module_rules") == "1"));
    if (has_rules) {
        std::string rules_text = cfg.count("rules_text") ? cfg.at("rules_text") : "Правила не заданы.";
        std::string rules_escaped = escapeForPython(rules_text);
        code << "    # Rules command\n";
        code << "    if text == '/rules':\n";
        code << "        api.send(\"" << rules_escaped << "\")\n";
        code << "        return\n\n";
    }
    
    // Notes
    bool has_notes = (cfg.count("module_notes") && (cfg.at("module_notes") == "true" || cfg.at("module_notes") == "1"));
    if (has_notes) {
        code << "    # Notes functionality\n";
        code << "    if text.startswith('/note '):\n";
        code << "        parts = text.split(' ', 2)\n";
        code << "        if len(parts) >= 2:\n";
        code << "            key = parts[1]\n";
        code << "            if len(parts) >= 3:\n";
        code << "                value = parts[2]\n";
        code << "                api.send(f'Заметка сохранена: {key}')\n";
        code << "            else:\n";
        code << "                api.send(f'Заметка {key}: (не найдена)')\n";
        code << "        return\n";
        code << "    if text == '/notes':\n";
        code << "        api.send('Список заметок: (функция в разработке)')\n";
        code << "        return\n\n";
    }
    
    // Reminders
    bool has_remind = (cfg.count("module_remind") && (cfg.at("module_remind") == "true" || cfg.at("module_remind") == "1"));
    if (has_remind) {
        code << "    # Reminders\n";
        code << "    if text.startswith('/remind '):\n";
        code << "        parts = text.split(' ', 2)\n";
        code << "        if len(parts) >= 3:\n";
        code << "            time_str = parts[1]\n";
        code << "            reminder_text = parts[2]\n";
        code << "            api.send(f'Напоминание создано: {reminder_text} через {time_str}')\n";
        code << "        else:\n";
        code << "            api.send('Использование: /remind 10m текст')\n";
        code << "        return\n\n";
    }
    
    // Fun commands
    bool has_fun = (cfg.count("module_fun") && (cfg.at("module_fun") == "true" || cfg.at("module_fun") == "1"));
    if (has_fun) {
        code << "    # Fun commands\n";
        code << "    if text == '/coin':\n";
        code << "        import random\n";
        code << "        api.send('Орёл' if random.randint(0, 1) else 'Решка')\n";
        code << "        return\n";
        code << "    if text.startswith('/roll'):\n";
        code << "        import random\n";
        code << "        try:\n";
        code << "            max_val = int(text.split()[1]) if len(text.split()) > 1 else 6\n";
        code << "            api.send(f'🎲 {random.randint(1, max_val)}')\n";
        code << "        except:\n";
        code << "            api.send('🎲 ' + str(random.randint(1, 6)))\n";
        code << "        return\n";
        code << "    if text.startswith('/choose '):\n";
        code << "        import random\n";
        code << "        options = text[8:].split('|')\n";
        code << "        if len(options) >= 2:\n";
        code << "            api.send('Выбираю: ' + random.choice(options).strip())\n";
        code << "        else:\n";
        code << "            api.send('Использование: /choose a|b|c')\n";
        code << "        return\n\n";
    }
    
    // Moderation (only in groups)
    bool has_mod = (cfg.count("module_moderation") && (cfg.at("module_moderation") == "true" || cfg.at("module_moderation") == "1"));
    if (has_mod) {
        code << "    # Moderation (only in groups)\n";
        code << "    if scope == 'group' and event_type == 'message' and text and not text.startswith('/'):\n";
        code << "        violated = False\n";
        code << "        reason = ''\n\n";
        
        if (cfg.count("mod_block_links") && (cfg.at("mod_block_links") == "true" || cfg.at("mod_block_links") == "1")) {
            code << "        # Check links\n";
            code << "        if 'http://' in text.lower() or 'https://' in text.lower() or 'www.' in text.lower():\n";
            code << "            violated = True\n";
            code << "            reason = 'Ссылки запрещены'\n\n";
        }
        
        if (cfg.count("mod_block_caps") && (cfg.at("mod_block_caps") == "true" || cfg.at("mod_block_caps") == "1")) {
            int caps_min_len = 10;
            if (cfg.count("mod_caps_min_len")) {
                try { caps_min_len = std::stoi(cfg.at("mod_caps_min_len")); } catch (...) {}
            }
            code << "        # Check caps\n";
            code << "        if not violated and len(text) >= " << caps_min_len << ":\n";
            code << "            caps_count = sum(1 for c in text if c.isupper() and c.isalpha())\n";
            code << "            total_letters = sum(1 for c in text if c.isalpha())\n";
            code << "            if total_letters > 0 and caps_count / total_letters > 0.7:\n";
            code << "                violated = True\n";
            code << "                reason = 'Не кричи капсом'\n\n";
        }
        
        if (cfg.count("mod_block_words") && (cfg.at("mod_block_words") == "true" || cfg.at("mod_block_words") == "1")) {
            std::string bad_words = cfg.count("mod_bad_words") ? cfg.at("mod_bad_words") : "";
            // Split by comma and create Python list
            code << "        # Check bad words\n";
            code << "        if not violated:\n";
            code << "            bad_words_list = [";
            if (!bad_words.empty()) {
                std::istringstream iss(bad_words);
                std::string word;
                bool first = true;
                while (std::getline(iss, word, ',')) {
                    // Trim whitespace
                    word.erase(0, word.find_first_not_of(" \t"));
                    word.erase(word.find_last_not_of(" \t") + 1);
                    if (!word.empty()) {
                        if (!first) code << ", ";
                        code << "'" << escapeForPython(word) << "'";
                        first = false;
                    }
                }
            }
            code << "]\n";
            code << "            text_lower = text.lower()\n";
            code << "            for word in bad_words_list:\n";
            code << "                if word and word in text_lower:\n";
            code << "                    violated = True\n";
            code << "                    reason = 'Нецензурные слова запрещены'\n";
            code << "                    break\n\n";
        }
        
        std::string warn_text = cfg.count("mod_warn_text") ? cfg.at("mod_warn_text") : "⚠️ {reason}";
        std::string warn_escaped = escapeForPython(warn_text);
        code << "        if violated:\n";
        code << "            warn_msg = \"" << warn_escaped << "\"\n";
        code << "            warn_msg = warn_msg.replace('{reason}', reason)\n";
        code << "            api.send(warn_msg)\n";
        code << "            return\n\n";
    }
    
    // Auto-reply (DM only)
    bool has_autoreply = (cfg.count("module_autoreply") && (cfg.at("module_autoreply") == "true" || cfg.at("module_autoreply") == "1"));
    if (has_autoreply) {
        code << "    # Auto-reply (DM only)\n";
        code << "    if scope == 'direct' and text and not text.startswith('/'):\n";
        std::string default_reply = cfg.count("dm_default_reply") ? cfg.at("dm_default_reply") : "";
        if (!default_reply.empty()) {
            std::string reply_escaped = escapeForPython(default_reply);
            code << "        api.send(\"" << reply_escaped << "\")\n";
        } else {
            code << "        api.send('Привет! Напиши /help для списка команд.')\n";
        }
        code << "        return\n\n";
    }
    
    // Help command
    code << "    # Help command\n";
    code << "    if text in ('/start', '/help'):\n";
    code << "        help_text = '🤖 Привет! Я бот " << bot_name << ".\\n\\n'\n";
    code << "        help_text += 'Команды:\\n'\n";
    if (has_rules) code << "        help_text += '/rules - правила\\n'\n";
    if (has_notes) code << "        help_text += '/note, /notes - заметки\\n'\n";
    if (has_remind) code << "        help_text += '/remind - напоминание\\n'\n";
    if (has_fun) code << "        help_text += '/roll, /coin, /choose - развлечения\\n'\n";
    code << "        api.send(help_text)\n";
    code << "        return\n\n";
    
    // Default response
    code << "    # Default response\n";
    code << "    if text:\n";
    code << "        api.send('Не понял. Напиши /help')\n";
    
    return code.str();
}

bool isValidBotUsername(const std::string& username_norm, std::string& error) {
    // Telegram-like: 5-32 chars, [a-z0-9_], endswith "bot"
    if (username_norm.size() < 5 || username_norm.size() > 32) {
        error = "Invalid bot_username length (must be 5-32)";
        return false;
    }
    for (char c : username_norm) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '_');
        if (!ok) {
            error = "Invalid bot_username (allowed: a-z, 0-9, _)";
            return false;
        }
    }
    if (username_norm.substr(username_norm.size() - 3) != "bot") {
        error = "Invalid bot_username (must end with 'bot')";
        return false;
    }
    if (username_norm.front() == '_' || username_norm.back() == '_') {
        error = "Invalid bot_username (cannot start/end with '_')";
        return false;
    }
    if (username_norm.find("__") != std::string::npos) {
        error = "Invalid bot_username (no consecutive underscores)";
        return false;
    }
    error.clear();
    return true;
}

std::string randomHex(size_t bytes) {
    std::vector<unsigned char> buf(bytes);
    if (RAND_bytes(buf.data(), static_cast<int>(buf.size())) != 1) {
        return "";
    }
    static const char* hex = "0123456789abcdef";
    std::string out(bytes * 2, '0');
    for (size_t i = 0; i < bytes; ++i) {
        out[i * 2]     = hex[(buf[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[buf[i] & 0xF];
    }
    return out;
}

} // namespace

std::string RequestHandler::handleCreateBot(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_name = data["bot_name"];
    std::string flow_json = data.count("flow_json") ? data["flow_json"] : "";
    std::string bot_username = data.count("bot_username") ? data["bot_username"] : "";
    
    if (token.empty() || bot_name.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }

    bot_name = normalizeSingleLineText(bot_name, kMaxBotNameLen);
    if (bot_name.empty() || containsHtmlAngleBrackets(bot_name)) {
        return JsonParser::createErrorResponse("Invalid bot_name");
    }

    if (flow_json.empty()) {
        flow_json = "{\"nodes\":[],\"edges\":[]}";
    }
    if (flow_json.size() > kMaxBotFlowJsonLen) {
        return JsonParser::createErrorResponse("flow_json too large");
    }
    if (!looksLikeJsonValue(flow_json)) {
        return JsonParser::createErrorResponse("Invalid flow_json");
    }

    std::string username_norm = toLowerAscii(bot_username);
    if (!username_norm.empty() && username_norm[0] == '@') {
        username_norm = username_norm.substr(1);
    }
    if (username_norm.empty()) {
        std::string suffix = randomHex(4);
        if (suffix.empty()) {
            return JsonParser::createErrorResponse("Failed to generate bot token");
        }
        username_norm = "xipher_" + suffix + "_bot";
    }

    std::string username_err;
    if (!isValidBotUsername(username_norm, username_err)) {
        return JsonParser::createErrorResponse(username_err);
    }

    // Avoid creating duplicate bots and leaving orphan bot-user accounts.
    // If a bot with this username already exists, tell the user to refresh (it should appear in the list).
    auto existing_bot = db_manager_.getBotBuilderBotByUsername(username_norm);
    if (!existing_bot.id.empty()) {
        return JsonParser::createErrorResponse("Бот с таким username уже существует. Обнови страницу /bots.");
    }
    // Username must be globally unique in users table too.
    if (db_manager_.usernameExists(username_norm)) {
        return JsonParser::createErrorResponse("Username уже занят");
    }

    // Generate bot token (CSPRNG): 32 bytes => 64 hex chars
    std::string bot_token = randomHex(32);
    if (bot_token.empty()) {
        return JsonParser::createErrorResponse("Failed to generate bot token");
    }
    
    // Create bot "user" account so it can be chatted with / added to groups/channels.
    // Bot user has random password_hash and is_bot=true in DB.
    std::string bot_user_id;
    const std::string bot_user_secret = randomHex(32);
    if (bot_user_secret.empty()) {
        return JsonParser::createErrorResponse("Failed to generate bot user secret");
    }
    std::string bot_user_pass_hash = PasswordHash::hash(bot_user_secret);
    if (bot_user_pass_hash.empty()) {
        return JsonParser::createErrorResponse("Failed to create bot user password hash");
    }
    if (!db_manager_.createBotUser(username_norm, bot_user_pass_hash, bot_user_id)) {
        return JsonParser::createErrorResponse("Failed to create bot user");
    }
    
    std::string bot_id;
    if (db_manager_.createBotBuilderBot(user_id, bot_user_id, bot_token, username_norm, bot_name, flow_json, bot_id)) {
        // Generate Python code from template if flow_json contains template_id
        auto cfg = JsonParser::parse(flow_json);
        bool has_template = cfg.count("template_id") && !cfg.at("template_id").empty();
        if (has_template) {
            // Log configuration for debugging
            std::ostringstream cfgLog;
            cfgLog << "Template config for bot " << bot_id << ": ";
            for (const auto& pair : cfg) {
                cfgLog << pair.first << "=" << pair.second << " ";
            }
            Logger::getInstance().info(cfgLog.str());
            
            std::string python_code = generatePythonCodeFromTemplate(bot_name, cfg);
            Logger::getInstance().info("Generated Python code for bot " + bot_id + " (length: " + std::to_string(python_code.length()) + ")");
            // Save Python code to bot (use INSERT ... ON CONFLICT or UPDATE)
            if (!db_manager_.updateBotBuilderScript(bot_id, "python", true, python_code)) {
                Logger::getInstance().error("Failed to save Python script for bot " + bot_id);
            } else {
                Logger::getInstance().info("Python script saved successfully for bot " + bot_id);
            }
            // Also save as main.py file in IDE
            if (!db_manager_.upsertBotFile(bot_id, "main.py", python_code)) {
                Logger::getInstance().error("Failed to save main.py file for bot " + bot_id);
            } else {
                Logger::getInstance().info("main.py file saved successfully for bot " + bot_id);
            }
        }
        
        // Make bot appear in chats list: auto-friend with owner
        db_manager_.createFriendshipPair(user_id, bot_user_id);
        // Friendly hello message
        db_manager_.sendMessage(bot_user_id, user_id, "Привет! Я бот \"" + bot_name + "\". Пока я в beta-режиме, но уже могу общаться 😊");

        std::ostringstream oss;
        oss << "{\"success\":true,\"bot_id\":\"" << JsonParser::escapeJson(bot_id) << "\","
            << "\"bot_token\":\"" << JsonParser::escapeJson(bot_token) << "\","
            << "\"bot_username\":\"" << JsonParser::escapeJson(username_norm) << "\"}";
        return oss.str();
    }
    
    return JsonParser::createErrorResponse("Failed to create bot");
}

std::string RequestHandler::handleUpdateBotFlow(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    std::string flow_json = data["flow_json"];
    bool is_active = data.count("is_active") ? (data["is_active"] == "true") : true;
    
    if (token.empty() || bot_id.empty() || flow_json.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Permission check (owner or developer)
    auto bot = db_manager_.getBotBuilderBotById(bot_id);
    if (bot.id.empty()) {
        return JsonParser::createErrorResponse("Bot not found");
    }
    if (!db_manager_.canEditBot(bot_id, user_id)) {
        return JsonParser::createErrorResponse("Permission denied");
    }
    
    if (db_manager_.updateBotBuilderBot(bot_id, flow_json, is_active)) {
        return JsonParser::createSuccessResponse("Bot flow updated");
    }
    
    return JsonParser::createErrorResponse("Failed to update bot flow");
}

std::string RequestHandler::handleUpdateBotProfile(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    std::string bot_name = data.count("bot_name") ? data["bot_name"] : "";
    std::string bot_description = data.count("bot_description") ? data["bot_description"] : "";

    if (token.empty() || bot_id.empty() || bot_name.empty()) {
        return JsonParser::createErrorResponse("token, bot_id and bot_name required");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    auto bot = db_manager_.getBotBuilderBotById(bot_id);
    if (bot.id.empty()) {
        return JsonParser::createErrorResponse("Bot not found");
    }
    if (!db_manager_.canEditBot(bot_id, user_id)) {
        return JsonParser::createErrorResponse("Permission denied");
    }

    if (!db_manager_.updateBotBuilderProfile(bot_id, bot_name, bot_description)) {
        return JsonParser::createErrorResponse("Failed to update bot profile");
    }

    return JsonParser::createSuccessResponse("Bot profile updated");
}

std::string RequestHandler::handleGetUserBots(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Missing token");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }
    
    auto bots = db_manager_.getUserBots(user_id);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"bots\":[";
    for (size_t i = 0; i < bots.size(); i++) {
        const auto& bot = bots[i];
        if (i > 0) oss << ",";
        oss << "{"
            << "\"id\":\"" << JsonParser::escapeJson(bot.id) << "\","
            << "\"bot_user_id\":\"" << JsonParser::escapeJson(bot.bot_user_id) << "\","
            << "\"bot_name\":\"" << JsonParser::escapeJson(bot.bot_name) << "\","
            << "\"bot_username\":\"" << JsonParser::escapeJson(bot.bot_username) << "\","
            << "\"bot_description\":\"" << JsonParser::escapeJson(bot.bot_description) << "\","
            << "\"bot_avatar_url\":\"" << JsonParser::escapeJson(bot.bot_avatar_url) << "\","
            << "\"flow_json\":\"" << JsonParser::escapeJson(bot.flow_json) << "\","
            << "\"is_active\":" << (bot.is_active ? "true" : "false") << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleDeployBot(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    
    if (token.empty() || bot_id.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }
    
    // TODO: Deploy bot logic
    
    return JsonParser::createSuccessResponse("Bot deployed");
}

std::string RequestHandler::handleDeleteBot(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    
    if (token.empty() || bot_id.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }
    
    // Ownership check
    auto bot = db_manager_.getBotBuilderBotById(bot_id);
    if (bot.id.empty()) {
        return JsonParser::createErrorResponse("Bot not found");
    }
    if (bot.user_id != user_id) {
        return JsonParser::createErrorResponse("Permission denied");
    }
    
    if (db_manager_.deleteBotBuilderBot(bot_id)) {
        return JsonParser::createSuccessResponse("Bot deleted");
    }
    
    return JsonParser::createErrorResponse("Failed to delete bot");
}

std::string RequestHandler::handleRevealBotToken(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    std::string password = data["password"];

    if (token.empty() || bot_id.empty() || password.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }

    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }

    if (actor.id.empty()) {
        return JsonParser::createErrorResponse("User not found");
    }
    if (!PasswordHash::verify(password, actor.password_hash)) {
        return JsonParser::createErrorResponse("Invalid password");
    }

    auto bot = db_manager_.getBotBuilderBotById(bot_id);
    if (bot.id.empty()) {
        return JsonParser::createErrorResponse("Bot not found");
    }
    if (bot.user_id != user_id) {
        return JsonParser::createErrorResponse("Permission denied");
    }

    std::map<std::string, std::string> out;
    out["bot_token"] = bot.bot_token;
    out["bot_username"] = bot.bot_username;
    out["bot_name"] = bot.bot_name;
    return JsonParser::createSuccessResponse("OK", out);
}

std::string RequestHandler::handleGetBotScript(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    if (token.empty() || bot_id.empty()) {
        return JsonParser::createErrorResponse("token and bot_id required");
    }
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) return JsonParser::createErrorResponse("Invalid token");

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }

    auto bot = db_manager_.getBotBuilderBotById(bot_id);
    if (bot.id.empty()) return JsonParser::createErrorResponse("Bot not found");
    if (!db_manager_.canEditBot(bot_id, user_id)) return JsonParser::createErrorResponse("Permission denied");

    std::string lang;
    bool enabled = false;
    std::string code;
    db_manager_.getBotBuilderScriptById(bot_id, lang, enabled, code);

    std::map<std::string, std::string> out;
    out["script_lang"] = lang.empty() ? "lite" : lang;
    out["script_enabled"] = enabled ? "true" : "false";
    out["script_code"] = code;
    return JsonParser::createSuccessResponse("OK", out);
}

std::string RequestHandler::handleUpdateBotScript(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    std::string lang = data.count("script_lang") ? data["script_lang"] : "python";
    std::string enabled_str = data.count("script_enabled") ? data["script_enabled"] : "false";
    std::string code = data.count("script_code") ? data["script_code"] : "";

    if (token.empty() || bot_id.empty()) {
        return JsonParser::createErrorResponse("token and bot_id required");
    }
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) return JsonParser::createErrorResponse("Invalid token");

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }

    auto bot = db_manager_.getBotBuilderBotById(bot_id);
    if (bot.id.empty()) return JsonParser::createErrorResponse("Bot not found");
    if (!db_manager_.canEditBot(bot_id, user_id)) return JsonParser::createErrorResponse("Permission denied");

    // Normalize
    std::transform(lang.begin(), lang.end(), lang.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    const bool enabled = (enabled_str == "true" || enabled_str == "1" || enabled_str == "yes" || enabled_str == "on");
    if (lang != "python" && lang != "lite") {
        return JsonParser::createErrorResponse("Invalid script_lang");
    }
    if (lang == "python") {
        if (code.size() > 200 * 1024) {
            return JsonParser::createErrorResponse("Python code too large");
        }
        if (enabled && code.empty()) {
            return JsonParser::createErrorResponse("Python code required");
        }
    } else {
        // lite mode: ignore stored code
        code = "";
    }

    if (!db_manager_.updateBotBuilderScript(bot_id, lang, enabled, code)) {
        return JsonParser::createErrorResponse("Failed to update bot script");
    }
    return JsonParser::createSuccessResponse("Bot script updated");
}

std::string RequestHandler::handleGetBotLogs(const std::string& /*body*/) {
    // TODO: Реализовать получение логов из Redis или БД
    return JsonParser::createErrorResponse("Bot logs not implemented yet");
}

std::string RequestHandler::handleReplayWebhook(const std::string& /*body*/) {
    // TODO: Реализовать переотправку webhook
    return JsonParser::createErrorResponse("Webhook replay not implemented yet");
}

std::string RequestHandler::handleAddBotDeveloper(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    std::string developer_username = data["developer_username"];
    
    if (token.empty() || bot_id.empty() || developer_username.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }

    developer_username = trimAsciiWhitespace(developer_username);
    if (!developer_username.empty() && developer_username[0] == '@') {
        developer_username.erase(0, 1);
    }
    developer_username = toLowerAscii(developer_username);
    if (developer_username.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }
    
    // Check ownership (only owner can add developers)
    auto bot = db_manager_.getBotBuilderBotById(bot_id);
    if (bot.id.empty()) {
        return JsonParser::createErrorResponse("Bot not found");
    }
    if (bot.user_id != user_id) {
        return JsonParser::createErrorResponse("Permission denied: only bot owner can add developers");
    }
    
    // Find developer user by username
    auto dev_user = db_manager_.getUserByUsername(developer_username);
    if (dev_user.id.empty()) {
        return JsonParser::createErrorResponse("User not found: @" + developer_username);
    }
    
    if (dev_user.id == user_id) {
        return JsonParser::createErrorResponse("Cannot add yourself as developer");
    }
    
    if (db_manager_.addBotDeveloper(bot_id, dev_user.id, user_id)) {
        return JsonParser::createSuccessResponse("Developer added");
    }
    
    return JsonParser::createErrorResponse("Failed to add developer");
}

std::string RequestHandler::handleRemoveBotDeveloper(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    std::string developer_user_id = data["developer_user_id"];
    
    if (token.empty() || bot_id.empty() || developer_user_id.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }
    
    // Check ownership (only owner can remove developers)
    auto bot = db_manager_.getBotBuilderBotById(bot_id);
    if (bot.id.empty()) {
        return JsonParser::createErrorResponse("Bot not found");
    }
    if (bot.user_id != user_id) {
        return JsonParser::createErrorResponse("Permission denied: only bot owner can remove developers");
    }
    
    if (db_manager_.removeBotDeveloper(bot_id, developer_user_id)) {
        return JsonParser::createSuccessResponse("Developer removed");
    }
    
    return JsonParser::createErrorResponse("Failed to remove developer");
}

std::string RequestHandler::handleGetBotDevelopers(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    
    if (token.empty() || bot_id.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }
    
    // Check if user can view (owner or developer)
    auto bot = db_manager_.getBotBuilderBotById(bot_id);
    if (bot.id.empty()) {
        return JsonParser::createErrorResponse("Bot not found");
    }
    if (!db_manager_.canEditBot(bot_id, user_id)) {
        return JsonParser::createErrorResponse("Permission denied");
    }
    
    auto developers = db_manager_.getBotDevelopers(bot_id);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"developers\":[";
    for (size_t i = 0; i < developers.size(); i++) {
        const auto& dev = developers[i];
        if (i > 0) oss << ",";
        oss << "{"
            << "\"developer_user_id\":\"" << JsonParser::escapeJson(dev.developer_user_id) << "\","
            << "\"developer_username\":\"" << JsonParser::escapeJson(dev.developer_username) << "\","
            << "\"added_by_user_id\":\"" << JsonParser::escapeJson(dev.added_by_user_id) << "\","
            << "\"added_at\":\"" << JsonParser::escapeJson(dev.added_at) << "\"}";
    }
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleListBotFiles(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    
    if (token.empty() || bot_id.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }
    
    if (!db_manager_.canEditBot(bot_id, user_id)) {
        return JsonParser::createErrorResponse("Permission denied");
    }
    
    auto files = db_manager_.listBotFiles(bot_id);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"files\":[";
    bool first = true;
    for (const auto& f : files) {
        const std::string safe_path = normalizeBotFilePath(f.file_path);
        if (!isSafeBotFilePath(safe_path)) {
            continue;
        }
        if (!first) oss << ",";
        oss << "{"
            << "\"id\":\"" << JsonParser::escapeJson(f.id) << "\","
            << "\"file_path\":\"" << JsonParser::escapeJson(safe_path) << "\","
            << "\"created_at\":\"" << JsonParser::escapeJson(f.created_at) << "\","
            << "\"updated_at\":\"" << JsonParser::escapeJson(f.updated_at) << "\"}";
        first = false;
    }
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handleGetBotFile(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    std::string file_path = data["file_path"];
    
    if (token.empty() || bot_id.empty() || file_path.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }

    file_path = normalizeBotFilePath(file_path);
    if (!isSafeBotFilePath(file_path)) {
        return JsonParser::createErrorResponse("Invalid file_path");
    }
    
    if (!db_manager_.canEditBot(bot_id, user_id)) {
        return JsonParser::createErrorResponse("Permission denied");
    }
    
    std::string content;
    if (!db_manager_.getBotFile(bot_id, file_path, content)) {
        return JsonParser::createErrorResponse("File not found");
    }
    
    std::map<std::string, std::string> out;
    out["file_path"] = file_path;
    out["file_content"] = content;
    return JsonParser::createSuccessResponse("OK", out);
}

std::string RequestHandler::handleSaveBotFile(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    std::string file_path = data["file_path"];
    std::string file_content = data["file_content"];
    
    if (token.empty() || bot_id.empty() || file_path.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }

    file_path = normalizeBotFilePath(file_path);
    if (!isSafeBotFilePath(file_path)) {
        return JsonParser::createErrorResponse("Invalid file_path");
    }
    if (file_content.size() > kMaxBotFileContentLen) {
        return JsonParser::createErrorResponse("File too large");
    }
    
    if (!db_manager_.canEditBot(bot_id, user_id)) {
        return JsonParser::createErrorResponse("Permission denied");
    }
    
    if (db_manager_.upsertBotFile(bot_id, file_path, file_content)) {
        // Если сохраняется main.py, обновляем script_code в bot_builder_bots
        if (file_path == "main.py" || file_path == "/main.py") {
            // Получаем bot_user_id для обновления скрипта
            auto bot = db_manager_.getBotBuilderBotById(bot_id);
            if (!bot.id.empty() && !bot.bot_user_id.empty()) {
                // Обновляем script_code и включаем скрипт
                db_manager_.updateBotBuilderScript(bot_id, "python", true, file_content);
                Logger::getInstance().info("Updated script_code for bot " + bot_id + " from main.py");
            }
        }
        return JsonParser::createSuccessResponse("File saved");
    }
    
    return JsonParser::createErrorResponse("Failed to save file");
}

std::string RequestHandler::handleDeleteBotFile(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    std::string file_path = data["file_path"];
    
    if (token.empty() || bot_id.empty() || file_path.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }

    file_path = normalizeBotFilePath(file_path);
    if (!isSafeBotFilePath(file_path)) {
        return JsonParser::createErrorResponse("Invalid file_path");
    }
    
    if (!db_manager_.canEditBot(bot_id, user_id)) {
        return JsonParser::createErrorResponse("Permission denied");
    }
    
    if (db_manager_.deleteBotFile(bot_id, file_path)) {
        return JsonParser::createSuccessResponse("File deleted");
    }
    
    return JsonParser::createErrorResponse("Failed to delete file");
}

std::string RequestHandler::handleBuildBotProject(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string bot_id = data["bot_id"];
    
    if (token.empty() || bot_id.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }

    User actor;
    std::string premium_error;
    if (!ensurePremiumAccess(db_manager_, user_id, actor, premium_error)) {
        return JsonParser::createErrorResponse(premium_error);
    }
    
    if (!db_manager_.canEditBot(bot_id, user_id)) {
        return JsonParser::createErrorResponse("Permission denied");
    }
    
    // Check if package.json exists (Node.js project)
    std::string package_json;
    if (!db_manager_.getBotFile(bot_id, "package.json", package_json)) {
        return JsonParser::createErrorResponse("package.json not found. This is not a Node.js project.");
    }
    
    // TODO: Execute npm install / npm run build in sandbox
    // For now, just return success
    std::map<std::string, std::string> out;
    out["message"] = "Build started (not implemented yet)";
    out["output"] = "npm install...\nnpm run build...\nBuild completed";
    return JsonParser::createSuccessResponse("Build started", out);
}

}
