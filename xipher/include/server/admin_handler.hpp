#ifndef ADMIN_HANDLER_HPP
#define ADMIN_HANDLER_HPP

#include <string>
#include <map>
#include <mutex>
#include <unordered_map>
#include "../database/db_manager.hpp"
#include "../auth/auth_manager.hpp"
#include "../security/admin_security.hpp"

namespace xipher {

class AdminHandler {
public:
    AdminHandler(DatabaseManager& db_manager, AuthManager& auth_manager);

    std::string handleLogin(const std::map<std::string, std::string>& headers, const std::string& body);
    std::string handleAction(const std::map<std::string, std::string>& headers, const std::string& body);

private:
    DatabaseManager& db_manager_;
    AuthManager& auth_manager_;
    RateLimiter login_rate_limiter_;
    RateLimiter shadow_ban_rate_limiter_;
    std::unordered_map<std::string, AdminSessionInfo> admin_sessions_;
    std::mutex sessions_mutex_;

    std::string hashContext(const std::string& input) const;
    bool validateAdminSession(const std::string& token, const std::string& ip, const std::string& user_agent);
    std::string buildHttpJson(const std::string& body, const std::map<std::string, std::string>& extra_headers = {});
};

} // namespace xipher

#endif // ADMIN_HANDLER_HPP
