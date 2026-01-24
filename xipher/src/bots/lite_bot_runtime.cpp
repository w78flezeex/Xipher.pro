#include "../include/bots/lite_bot_runtime.hpp"
#include "../include/bots/python_bot_executor.hpp"
#include "../include/utils/json_parser.hpp"
#include "../include/utils/logger.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace xipher {

namespace {

static std::string ltrim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    return s;
}

static std::string rtrim(std::string s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

static std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

static std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream iss(s);
    while (std::getline(iss, cur, delim)) out.push_back(cur);
    return out;
}

static bool truthy(const std::map<std::string, std::string>& cfg, const std::string& key) {
    auto it = cfg.find(key);
    if (it == cfg.end()) return false;
    const std::string v = toLower(trim(it->second));
    return v == "1" || v == "true" || v == "yes" || v == "on";
}

static std::string getStr(const std::map<std::string, std::string>& cfg, const std::string& key, const std::string& def = "") {
    auto it = cfg.find(key);
    if (it == cfg.end()) return def;
    return it->second;
}

static std::string expandEscapes(std::string s) {
    // Convert stored "\\n" sequences to real newlines for user-facing messages.
    // We store literal backslashes in JSON config to avoid breaking Postgres json parsing
    // due to the server's flat JsonParser unescaping once.
    size_t pos = 0;
    while ((pos = s.find("\\n", pos)) != std::string::npos) {
        s.replace(pos, 2, "\n");
        pos += 1;
    }
    pos = 0;
    while ((pos = s.find("\\t", pos)) != std::string::npos) {
        s.replace(pos, 2, "\t");
        pos += 1;
    }
    return s;
}

static int getInt(const std::map<std::string, std::string>& cfg, const std::string& key, int def) {
    auto it = cfg.find(key);
    if (it == cfg.end()) return def;
    try { return std::stoi(it->second); } catch (...) { return def; }
}

static std::string getUsername(DatabaseManager& db, const std::string& user_id) {
    if (user_id.empty()) return "";
    auto user = db.getUserById(user_id);
    return user.username;
}

static bool containsLink(const std::string& text) {
    const std::string t = toLower(text);
    return t.find("http://") != std::string::npos ||
           t.find("https://") != std::string::npos ||
           t.find("t.me/") != std::string::npos ||
           t.find("www.") != std::string::npos;
}

static double capsRatio(const std::string& text) {
    int letters = 0;
    int caps = 0;
    for (unsigned char c : text) {
        if (std::isalpha(c)) {
            letters++;
            if (std::isupper(c)) caps++;
        }
    }
    if (letters == 0) return 0.0;
    return static_cast<double>(caps) / static_cast<double>(letters);
}

static std::string helpText(const std::map<std::string, std::string>& cfg, bool in_group) {
    // Allow fully custom help text (any wording)
    if (in_group) {
        const std::string h = expandEscapes(getStr(cfg, "help_text_group", ""));
        if (!trim(h).empty()) return h;
    }
    {
        const std::string h = expandEscapes(getStr(cfg, "help_text", ""));
        if (!trim(h).empty()) return h;
    }

    std::ostringstream oss;
    oss << "Команды бота:\n";
    oss << "/help — помощь\n";
    oss << "/start — старт\n";
    if (truthy(cfg, "module_rules")) {
        oss << "/rules — правила\n";
    }
    if (truthy(cfg, "module_notes")) {
        oss << "/note <key> <text> — сохранить заметку\n";
        oss << "/note <key> — показать заметку\n";
        oss << "/notes — список заметок\n";
        oss << "/delnote <key> — удалить заметку\n";
    }
    if (truthy(cfg, "module_remind")) {
        oss << "/remind <10m|2h|1d> <text> — напоминание\n";
    }
    if (truthy(cfg, "module_fun")) {
        oss << "/roll [N] — рандом 1..N\n";
        oss << "/coin — орёл/решка\n";
        oss << "/choose a|b|c — выбрать\n";
    }
    if (in_group && truthy(cfg, "module_moderation")) {
        oss << "\nМодерация включена.\n";
    }
    return oss.str();
}

static int parseDurationSeconds(const std::string& token) {
    std::string t = toLower(trim(token));
    if (t.empty()) return -1;
    char suffix = t.back();
    int mult = 1;
    if (!std::isdigit(static_cast<unsigned char>(suffix))) {
        if (suffix == 's') mult = 1;
        else if (suffix == 'm') mult = 60;
        else if (suffix == 'h') mult = 60 * 60;
        else if (suffix == 'd') mult = 60 * 60 * 24;
        else return -1;
        t.pop_back();
    }
    if (t.empty()) return -1;
    int n = 0;
    try { n = std::stoi(t); } catch (...) { return -1; }
    if (n <= 0) return -1;
    return n * mult;
}

} // namespace

std::map<std::string, std::string> LiteBotRuntime::getConfig(const DatabaseManager::BotBuilderBot& bot) {
    // flow_json is stored as jsonb, but we keep it a flat JSON string for configs
    auto cfg = JsonParser::parse(bot.flow_json);
    if (cfg.empty()) {
        // Backward compatibility: old bots may have {"nodes":[],"edges":[]} — keep defaults
        cfg["template_id"] = "custom";
    }
    return cfg;
}

void LiteBotRuntime::onDirectMessage(DatabaseManager& db,
                                     const DatabaseManager::BotBuilderBot& bot,
                                     const std::string& from_user_id,
                                     const std::string& text) {
    if (!bot.is_active) return;
    if (from_user_id.empty()) return;
    if (from_user_id == bot.bot_user_id) return; // avoid loops

    // Python script mode (full code)
    {
        PythonBotEvent ev;
        ev.type = "message";
        ev.scope = "direct";
        ev.scope_id = from_user_id;
        ev.from_user_id = from_user_id;
        ev.from_username = getUsername(db, from_user_id);
        ev.from_role = "";
        ev.text = text;
        if (PythonBotExecutor::handle(db, bot.bot_user_id, ev)) {
            return;
        }
    }

    auto cfg = getConfig(bot);
    const std::string msg = trim(text);
    if (msg.empty()) return;

    // simple keyword auto-replies (DM only)
    if (truthy(cfg, "module_autoreply") && msg[0] != '/') {
        // format: "hi=hello;bye=goodbye"
        const std::string rules = getStr(cfg, "autoreply_rules", "");
        for (const auto& item : split(rules, ';')) {
            auto kv = split(item, '=');
            if (kv.size() < 2) continue;
            const std::string k = toLower(trim(kv[0]));
            const std::string v = trim(item.substr(item.find('=') + 1));
            if (!k.empty() && toLower(msg).find(k) != std::string::npos) {
                db.sendMessage(bot.bot_user_id, from_user_id, expandEscapes(v));
                return;
            }
        }
    }

    // Commands
    if (msg[0] != '/') {
        const std::string default_reply = expandEscapes(getStr(cfg, "dm_default_reply", ""));
        if (!default_reply.empty()) {
            db.sendMessage(bot.bot_user_id, from_user_id, default_reply);
        }
        return;
    }

    auto parts = split(msg, ' ');
    const std::string cmd = toLower(parts.empty() ? "" : parts[0]);
    const std::string cmd_name = (cmd.size() > 1 && cmd[0] == '/') ? cmd.substr(1) : "";

    // Custom commands: cfg key "cmd_<name>" -> reply
    if (!cmd_name.empty()) {
        const std::string key = "cmd_" + cmd_name;
        auto it = cfg.find(key);
        if (it != cfg.end()) {
            const std::string reply = expandEscapes(trim(it->second));
            if (!reply.empty()) {
                db.sendMessage(bot.bot_user_id, from_user_id, reply);
                return;
            }
        }
    }

    if (cmd == "/start" || cmd == "/help") {
        db.sendMessage(bot.bot_user_id, from_user_id, helpText(cfg, false));
        return;
    }

    if (cmd == "/rules" && truthy(cfg, "module_rules")) {
        const std::string rules = expandEscapes(getStr(cfg, "rules_text", "Правила не заданы."));
        db.sendMessage(bot.bot_user_id, from_user_id, rules);
        return;
    }

    if (truthy(cfg, "module_fun")) {
        if (cmd == "/coin") {
            const int r = std::rand() % 2;
            db.sendMessage(bot.bot_user_id, from_user_id, r ? "Орёл" : "Решка");
            return;
        }
        if (cmd == "/roll") {
            int maxv = 6;
            if (parts.size() >= 2) {
                try { maxv = std::max(2, std::min(100000, std::stoi(parts[1]))); } catch (...) {}
            }
            const int r = (std::rand() % maxv) + 1;
            db.sendMessage(bot.bot_user_id, from_user_id, "🎲 " + std::to_string(r));
            return;
        }
        if (cmd == "/choose") {
            std::string rest = trim(msg.substr(std::min(msg.size(), cmd.size())));
            rest = trim(rest);
            auto opts = split(rest, '|');
            std::vector<std::string> cleaned;
            for (auto& o : opts) {
                o = trim(o);
                if (!o.empty()) cleaned.push_back(o);
            }
            if (cleaned.size() < 2) {
                db.sendMessage(bot.bot_user_id, from_user_id, "Формат: /choose a|b|c");
                return;
            }
            const int r = std::rand() % static_cast<int>(cleaned.size());
            db.sendMessage(bot.bot_user_id, from_user_id, "Выбираю: " + cleaned[r]);
            return;
        }
    }

    if (truthy(cfg, "module_notes")) {
        // Notes scoped by DM partner (from_user_id)
        const std::string scope_type = "direct";
        const std::string scope_id = from_user_id;

        if (cmd == "/notes") {
            std::vector<std::string> keys;
            if (!db.listBotNotes(bot.bot_user_id, scope_type, scope_id, keys)) {
                db.sendMessage(bot.bot_user_id, from_user_id, "Не удалось получить заметки.");
                return;
            }
            if (keys.empty()) {
                db.sendMessage(bot.bot_user_id, from_user_id, "Пока нет заметок.");
                return;
            }
            std::ostringstream oss;
            oss << "Заметки:\n";
            for (const auto& k : keys) oss << "- " << k << "\n";
            db.sendMessage(bot.bot_user_id, from_user_id, oss.str());
            return;
        }

        if (cmd == "/delnote" && parts.size() >= 2) {
            const std::string key = trim(parts[1]);
            if (key.empty()) {
                db.sendMessage(bot.bot_user_id, from_user_id, "Формат: /delnote <key>");
                return;
            }
            if (!db.deleteBotNote(bot.bot_user_id, scope_type, scope_id, key)) {
                db.sendMessage(bot.bot_user_id, from_user_id, "Не удалось удалить (возможно нет такой).");
                return;
            }
            db.sendMessage(bot.bot_user_id, from_user_id, "Удалено: " + key);
            return;
        }

        if (cmd == "/note") {
            if (parts.size() < 2) {
                db.sendMessage(bot.bot_user_id, from_user_id, "Формат: /note <key> <text> или /note <key>");
                return;
            }
            const std::string key = trim(parts[1]);
            if (key.empty()) {
                db.sendMessage(bot.bot_user_id, from_user_id, "Формат: /note <key> <text>");
                return;
            }
            // get or set
            std::string rest;
            if (msg.size() > cmd.size() + 1 + parts[1].size()) {
                rest = trim(msg.substr(cmd.size() + 1 + parts[1].size()));
            }
            if (rest.empty()) {
                std::string value;
                if (!db.getBotNote(bot.bot_user_id, scope_type, scope_id, key, value)) {
                    db.sendMessage(bot.bot_user_id, from_user_id, "Не найдено: " + key);
                    return;
                }
                db.sendMessage(bot.bot_user_id, from_user_id, value);
                return;
            }
            if (!db.upsertBotNote(bot.bot_user_id, scope_type, scope_id, key, rest, from_user_id)) {
                db.sendMessage(bot.bot_user_id, from_user_id, "Не удалось сохранить заметку.");
                return;
            }
            db.sendMessage(bot.bot_user_id, from_user_id, "Сохранено: " + key);
            return;
        }
    }

    if (truthy(cfg, "module_remind") && cmd == "/remind") {
        if (parts.size() < 3) {
            db.sendMessage(bot.bot_user_id, from_user_id, "Формат: /remind <10m|2h|1d> <text>");
            return;
        }
        const int sec = parseDurationSeconds(parts[1]);
        if (sec <= 0) {
            db.sendMessage(bot.bot_user_id, from_user_id, "Неверное время. Пример: 10m, 2h, 1d");
            return;
        }
        std::string reminder_text;
        for (size_t i = 2; i < parts.size(); i++) {
            if (!reminder_text.empty()) reminder_text += " ";
            reminder_text += parts[i];
        }
        reminder_text = trim(reminder_text);
        if (reminder_text.empty()) {
            db.sendMessage(bot.bot_user_id, from_user_id, "Укажи текст напоминания.");
            return;
        }
        std::string reminder_id;
        if (!db.createBotReminder(bot.bot_user_id, "direct", from_user_id, from_user_id, reminder_text, sec, reminder_id)) {
            db.sendMessage(bot.bot_user_id, from_user_id, "Не удалось создать напоминание.");
            return;
        }
        db.sendMessage(bot.bot_user_id, from_user_id, "Ок, напомню через " + parts[1] + ".");
        return;
    }

    // Unknown command fallback (customizable)
    {
        const std::string unknown = expandEscapes(getStr(cfg, "unknown_command_reply", ""));
        if (!trim(unknown).empty()) {
            db.sendMessage(bot.bot_user_id, from_user_id, unknown);
            return;
        }
    }
    db.sendMessage(bot.bot_user_id, from_user_id, "Не понял. Напиши /help");
}

void LiteBotRuntime::onGroupMessage(DatabaseManager& db,
                                    const DatabaseManager::BotBuilderBot& bot,
                                    const std::string& group_id,
                                    const std::string& from_user_id,
                                    const std::string& from_role,
                                    const std::string& text) {
    if (!bot.is_active) return;
    if (group_id.empty() || from_user_id.empty()) return;
    if (from_user_id == bot.bot_user_id) return; // avoid loops

    // Python script mode (full code)
    {
        PythonBotEvent ev;
        ev.type = "message";
        ev.scope = "group";
        ev.scope_id = group_id;
        ev.from_user_id = from_user_id;
        ev.from_username = getUsername(db, from_user_id);
        ev.from_role = from_role;
        ev.text = text;
        if (PythonBotExecutor::handle(db, bot.bot_user_id, ev)) {
            return;
        }
    }

    auto cfg = getConfig(bot);
    const std::string msg = trim(text);
    if (msg.empty()) return;

    const bool is_admin = (from_role == "admin" || from_role == "creator");

    // Moderation (non-destructive): warn + optional mute
    if (truthy(cfg, "module_moderation") && !is_admin && msg[0] != '/') {
        const bool block_links = truthy(cfg, "mod_block_links");
        const bool block_caps = truthy(cfg, "mod_block_caps");
        const bool block_words = truthy(cfg, "mod_block_words");

        bool violated = false;
        std::string reason;

        if (block_links && containsLink(msg)) {
            violated = true;
            reason = "Ссылки запрещены";
        }

        if (!violated && block_caps) {
            const double r = capsRatio(msg);
            const int min_len = getInt(cfg, "mod_caps_min_len", 12);
            const double thr = 0.75;
            if (static_cast<int>(msg.size()) >= min_len && r >= thr) {
                violated = true;
                reason = "Не кричи капсом";
            }
        }

        if (!violated && block_words) {
            const std::string words = getStr(cfg, "mod_bad_words", "");
            const std::string low = toLower(msg);
            for (const auto& w0 : split(words, ',')) {
                const std::string w = toLower(trim(w0));
                if (w.empty()) continue;
                if (low.find(w) != std::string::npos) {
                    violated = true;
                    reason = "Нецензурные слова запрещены";
                    break;
                }
            }
        }

        if (violated) {
            const std::string warn_tpl = expandEscapes(getStr(cfg, "mod_warn_text", "⚠️ {reason}, @{username}"));
            // We don't have username here cheaply; keep it simple.
            std::string warn = warn_tpl;
            auto pos = warn.find("{reason}");
            if (pos != std::string::npos) warn.replace(pos, 8, reason);
            db.sendGroupMessage(group_id, bot.bot_user_id, warn);

            if (truthy(cfg, "mod_auto_mute")) {
                db.muteGroupMember(group_id, from_user_id, true);
                db.sendGroupMessage(group_id, bot.bot_user_id, "🔇 Пользователь замьючен (автомод).");
            }
        }
    }

    // Commands in group
    if (msg[0] != '/') return;

    auto parts = split(msg, ' ');
    const std::string cmd = toLower(parts.empty() ? "" : parts[0]);
    const std::string cmd_name = (cmd.size() > 1 && cmd[0] == '/') ? cmd.substr(1) : "";

    // Custom commands: cfg key "cmd_<name>" -> reply
    if (!cmd_name.empty()) {
        const std::string key = "cmd_" + cmd_name;
        auto it = cfg.find(key);
        if (it != cfg.end()) {
            const std::string reply = expandEscapes(trim(it->second));
            if (!reply.empty()) {
                db.sendGroupMessage(group_id, bot.bot_user_id, reply);
                return;
            }
        }
    }

    if (cmd == "/help" || cmd == "/start") {
        db.sendGroupMessage(group_id, bot.bot_user_id, helpText(cfg, true));
        return;
    }

    if (cmd == "/rules" && truthy(cfg, "module_rules")) {
        const std::string rules = expandEscapes(getStr(cfg, "rules_text", "Правила не заданы."));
        db.sendGroupMessage(group_id, bot.bot_user_id, rules);
        return;
    }

    if (truthy(cfg, "module_notes")) {
        const std::string scope_type = "group";
        const std::string scope_id = group_id;

        if (cmd == "/notes") {
            std::vector<std::string> keys;
            if (!db.listBotNotes(bot.bot_user_id, scope_type, scope_id, keys)) {
                db.sendGroupMessage(group_id, bot.bot_user_id, "Не удалось получить заметки.");
                return;
            }
            if (keys.empty()) {
                db.sendGroupMessage(group_id, bot.bot_user_id, "Пока нет заметок.");
                return;
            }
            std::ostringstream oss;
            oss << "Заметки:\n";
            for (const auto& k : keys) oss << "- " << k << "\n";
            db.sendGroupMessage(group_id, bot.bot_user_id, oss.str());
            return;
        }

        if (cmd == "/delnote") {
            if (!is_admin) {
                db.sendGroupMessage(group_id, bot.bot_user_id, "Только админы могут удалять заметки.");
                return;
            }
            if (parts.size() < 2) {
                db.sendGroupMessage(group_id, bot.bot_user_id, "Формат: /delnote <key>");
                return;
            }
            const std::string key = trim(parts[1]);
            if (!db.deleteBotNote(bot.bot_user_id, scope_type, scope_id, key)) {
                db.sendGroupMessage(group_id, bot.bot_user_id, "Не удалось удалить (возможно нет такой).");
                return;
            }
            db.sendGroupMessage(group_id, bot.bot_user_id, "Удалено: " + key);
            return;
        }

        if (cmd == "/note") {
            if (parts.size() < 2) {
                db.sendGroupMessage(group_id, bot.bot_user_id, "Формат: /note <key> <text> или /note <key>");
                return;
            }
            const std::string key = trim(parts[1]);
            if (key.empty()) {
                db.sendGroupMessage(group_id, bot.bot_user_id, "Формат: /note <key> <text>");
                return;
            }
            std::string rest;
            if (msg.size() > cmd.size() + 1 + parts[1].size()) {
                rest = trim(msg.substr(cmd.size() + 1 + parts[1].size()));
            }
            if (rest.empty()) {
                std::string value;
                if (!db.getBotNote(bot.bot_user_id, scope_type, scope_id, key, value)) {
                    db.sendGroupMessage(group_id, bot.bot_user_id, "Не найдено: " + key);
                    return;
                }
                db.sendGroupMessage(group_id, bot.bot_user_id, value);
                return;
            }
            if (!is_admin) {
                db.sendGroupMessage(group_id, bot.bot_user_id, "Только админы могут менять заметки.");
                return;
            }
            if (!db.upsertBotNote(bot.bot_user_id, scope_type, scope_id, key, rest, from_user_id)) {
                db.sendGroupMessage(group_id, bot.bot_user_id, "Не удалось сохранить заметку.");
                return;
            }
            db.sendGroupMessage(group_id, bot.bot_user_id, "Сохранено: " + key);
            return;
        }
    }

    if (truthy(cfg, "module_remind") && cmd == "/remind") {
        if (parts.size() < 3) {
            db.sendGroupMessage(group_id, bot.bot_user_id, "Формат: /remind <10m|2h|1d> <text>");
            return;
        }
        const int sec = parseDurationSeconds(parts[1]);
        if (sec <= 0) {
            db.sendGroupMessage(group_id, bot.bot_user_id, "Неверное время. Пример: 10m, 2h, 1d");
            return;
        }
        std::string remainder;
        for (size_t i = 2; i < parts.size(); i++) {
            if (!remainder.empty()) remainder += " ";
            remainder += parts[i];
        }
        remainder = trim(remainder);
        if (remainder.empty()) {
            db.sendGroupMessage(group_id, bot.bot_user_id, "Укажи текст напоминания.");
            return;
        }
        std::string reminder_id;
        if (!db.createBotReminder(bot.bot_user_id, "group", group_id, from_user_id, remainder, sec, reminder_id)) {
            db.sendGroupMessage(group_id, bot.bot_user_id, "Не удалось создать напоминание.");
            return;
        }
        db.sendGroupMessage(group_id, bot.bot_user_id, "Ок, напомню через " + parts[1] + ".");
        return;
    }

    // Unknown command fallback (optional in groups)
    {
        const std::string unknown = expandEscapes(getStr(cfg, "unknown_command_reply", ""));
        if (!trim(unknown).empty()) {
            db.sendGroupMessage(group_id, bot.bot_user_id, unknown);
        }
    }
}

void LiteBotRuntime::onGroupMemberJoined(DatabaseManager& db,
                                         const DatabaseManager::BotBuilderBot& bot,
                                         const std::string& group_id,
                                         const std::string& joined_user_id,
                                         const std::string& joined_username) {
    if (!bot.is_active) return;
    // Python script mode (full code)
    {
        PythonBotEvent ev;
        ev.type = "member_joined";
        ev.scope = "group";
        ev.scope_id = group_id;
        ev.from_user_id = joined_user_id;
        ev.from_username = joined_username.empty() ? getUsername(db, joined_user_id) : joined_username;
        ev.from_role = "";
        ev.text = "";
        ev.joined_username = joined_username;
        if (PythonBotExecutor::handle(db, bot.bot_user_id, ev)) {
            return;
        }
    }

    if (!truthy(getConfig(bot), "module_welcome")) return;
    if (group_id.empty()) return;
    if (joined_user_id == bot.bot_user_id) return;

    auto cfg = getConfig(bot);
    std::string msg = expandEscapes(getStr(cfg, "welcome_text", "Привет, @{username}! Добро пожаловать 👋"));
    auto pos = msg.find("{username}");
    if (pos != std::string::npos) msg.replace(pos, 10, joined_username.empty() ? "user" : joined_username);
    // Support both {username} and @{username}
    pos = msg.find("@{username}");
    if (pos != std::string::npos) msg.replace(pos, 11, joined_username.empty() ? "user" : joined_username);
    db.sendGroupMessage(group_id, bot.bot_user_id, msg);
}

} // namespace xipher
