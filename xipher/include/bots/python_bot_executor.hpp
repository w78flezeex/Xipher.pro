#ifndef XIPHER_PYTHON_BOT_EXECUTOR_HPP
#define XIPHER_PYTHON_BOT_EXECUTOR_HPP

#include <string>
#include <map>
#include "../database/db_manager.hpp"

namespace xipher {

struct PythonBotEvent {
    std::string type;       // "message" | "member_joined"
    std::string scope;      // "direct" | "group"
    std::string scope_id;   // user_id (direct) | group_id (group)
    std::string from_user_id;
    std::string from_username;
    std::string from_role;  // group role of sender (creator/admin/member)
    std::string text;       // message text
    std::string joined_username; // for member_joined
};

class PythonBotExecutor {
public:
    // Returns true if the script was executed (even if it produced no actions).
    static bool handle(DatabaseManager& db,
                       const std::string& bot_user_id,
                       const PythonBotEvent& ev);
};

} // namespace xipher

#endif // XIPHER_PYTHON_BOT_EXECUTOR_HPP

