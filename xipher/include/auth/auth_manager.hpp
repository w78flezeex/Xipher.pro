#ifndef AUTH_MANAGER_HPP
#define AUTH_MANAGER_HPP

#include <string>
#include <map>
#include <mutex>
#include "../database/db_manager.hpp"

namespace xipher {

class AuthManager {
public:
    AuthManager(DatabaseManager& db_manager);
    
    // Registration
    std::map<std::string, std::string> registerUser(const std::string& username, const std::string& password);
    
    // Authentication
    std::map<std::string, std::string> login(const std::string& username, const std::string& password);
    
    // Session management
    std::string generateSessionToken(const std::string& user_id);
    bool validateSessionToken(const std::string& token);
    std::string getUserIdFromToken(const std::string& token);
    std::string rotateSessionToken(const std::string& token);
    bool revokeSessionToken(const std::string& token);
    
    // Validation
    bool validateUsername(const std::string& username);
    bool validatePassword(const std::string& password);
    
private:
    DatabaseManager& db_manager_;
    std::map<std::string, std::string> sessions_; // token -> user_id
    std::mutex sessions_mutex_;
    
    std::string generateRandomToken();
};

} // namespace xipher

#endif // AUTH_MANAGER_HPP
