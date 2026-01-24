#ifndef XIPHER_LITE_BOT_RUNTIME_HPP
#define XIPHER_LITE_BOT_RUNTIME_HPP

#include <string>
#include <map>
#include "../database/db_manager.hpp"

namespace xipher {

// Lightweight bot runtime driven by Bot Builder's flow_json (stored as flat JSON string map).
// Works for:
// - Direct messages (user -> bot)
// - Group messages (user -> group, bots that are members react)
//
// Important: JsonParser in this codebase is "flat"; keep bot configs as simple key/value pairs.
class LiteBotRuntime {
public:
    static void onDirectMessage(DatabaseManager& db,
                                const DatabaseManager::BotBuilderBot& bot,
                                const std::string& from_user_id,
                                const std::string& text);

    static void onGroupMessage(DatabaseManager& db,
                               const DatabaseManager::BotBuilderBot& bot,
                               const std::string& group_id,
                               const std::string& from_user_id,
                               const std::string& from_role,
                               const std::string& text);

    static void onGroupMemberJoined(DatabaseManager& db,
                                    const DatabaseManager::BotBuilderBot& bot,
                                    const std::string& group_id,
                                    const std::string& joined_user_id,
                                    const std::string& joined_username);

private:
    static std::map<std::string, std::string> getConfig(const DatabaseManager::BotBuilderBot& bot);
};

} // namespace xipher

#endif // XIPHER_LITE_BOT_RUNTIME_HPP


