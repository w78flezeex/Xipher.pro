#include "../include/auth/auth_manager.hpp"
#include "../include/auth/password_hash.hpp"
#include "../include/utils/logger.hpp"
#include "../include/utils/json_parser.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <openssl/rand.h>

namespace xipher {

AuthManager::AuthManager(DatabaseManager& db_manager) : db_manager_(db_manager) {
}

std::map<std::string, std::string> AuthManager::registerUser(const std::string& username, const std::string& password) {
    std::map<std::string, std::string> result;
    
    // Validate input
    if (!validateUsername(username)) {
        result["success"] = "false";
        result["message"] = "Invalid username. Must be 3-50 characters, alphanumeric and underscores only.";
        return result;
    }
    
    if (!validatePassword(password)) {
        result["success"] = "false";
        result["message"] = "Invalid password. Must be at least 6 characters.";
        return result;
    }
    
    // Check if username exists
    if (db_manager_.usernameExists(username)) {
        result["success"] = "false";
        result["message"] = "Username already taken";
        return result;
    }
    
    // Hash password
    std::string password_hash = PasswordHash::hash(password);
    if (password_hash.empty()) {
        result["success"] = "false";
        result["message"] = "Failed to hash password";
        return result;
    }
    
    // Create user
    if (!db_manager_.createUser(username, password_hash)) {
        result["success"] = "false";
        result["message"] = "Failed to create user";
        return result;
    }
    
    result["success"] = "true";
    result["message"] = "User registered successfully";
    return result;
}

std::map<std::string, std::string> AuthManager::login(const std::string& username, const std::string& password) {
    std::map<std::string, std::string> result;
    
    // Get user
    User user = db_manager_.getUserByUsername(username);
    if (user.id.empty()) {
        result["success"] = "false";
        result["message"] = "Invalid username or password";
        return result;
    }
    
    // Verify password
    if (!PasswordHash::verify(password, user.password_hash)) {
        result["success"] = "false";
        result["message"] = "Invalid username or password";
        return result;
    }

    // Upgrade legacy hashes to Argon2id on successful login
    if (PasswordHash::needsRehash(user.password_hash)) {
        std::string upgraded_hash = PasswordHash::hash(password);
        if (!upgraded_hash.empty()) {
            db_manager_.updateUserPasswordHash(user.id, upgraded_hash);
        }
    }
    
    // Check if user is active
    if (!user.is_active) {
        result["success"] = "false";
        result["message"] = "Account is disabled";
        return result;
    }
    
    // Update last login
    db_manager_.updateLastLogin(user.id);
    
    // Generate session token
    std::string token = generateSessionToken(user.id);
    if (token.empty()) {
        result["success"] = "false";
        result["message"] = "Failed to create session";
        return result;
    }
    
    result["success"] = "true";
    result["message"] = "Login successful";
    result["token"] = token;
    result["user_id"] = user.id;
    result["username"] = user.username;
    return result;
}

std::string AuthManager::generateSessionToken(const std::string& user_id) {
    std::string token = generateRandomToken();
    if (token.empty()) {
        return "";
    }
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_[token] = user_id;
    
    // Persist session to DB so it survives restarts (best-effort)
    if (!db_manager_.upsertUserSession(token, user_id)) {
        Logger::getInstance().warning("Failed to persist session token to DB (will be in-memory only until next login)");
    }

    Logger::getInstance().info("Generated token for user_id: " + user_id + ", token: " + token.substr(0, 10) + "...");
    
    return token;
}

bool AuthManager::validateSessionToken(const std::string& token) {
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        if (sessions_.find(token) != sessions_.end()) {
            return true;
        }
    }
    // Fallback to DB
    std::string user_id = db_manager_.getUserIdBySessionToken(token);
    if (!user_id.empty()) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[token] = user_id;
        return true;
    }
    return false;
}

std::string AuthManager::getUserIdFromToken(const std::string& token) {
    if (token.empty()) {
        return "";
    }
    
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = sessions_.find(token);
        if (it != sessions_.end()) {
            return it->second;
        }
    }

    // Fallback to DB (persisted sessions)
    std::string user_id = db_manager_.getUserIdBySessionToken(token);
    if (!user_id.empty()) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[token] = user_id;
        return user_id;
    }

    // Security: Never log tokens or token fragments
    // Only log that authentication failed (without sensitive data)
    Logger::getInstance().debug("Session token validation failed");
    return "";
}

std::string AuthManager::rotateSessionToken(const std::string& token) {
    if (token.empty()) {
        return "";
    }
    std::string user_id = getUserIdFromToken(token);
    if (user_id.empty()) {
        return "";
    }

    std::string new_token = generateRandomToken();
    if (new_token.empty()) {
        return "";
    }
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(token);
        sessions_[new_token] = user_id;
    }

    db_manager_.deleteUserSession(token);
    db_manager_.upsertUserSession(new_token, user_id);

    return new_token;
}

bool AuthManager::revokeSessionToken(const std::string& token) {
    if (token.empty()) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_.erase(token);
    }
    return db_manager_.deleteUserSession(token);
}

bool AuthManager::validateUsername(const std::string& username) {
    if (username.length() < 3 || username.length() > 50) {
        return false;
    }
    
    for (char c : username) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }
    
    return true;
}

bool AuthManager::validatePassword(const std::string& password) {
    // Security: Enforce strong password policy (OWASP recommendations)
    // Minimum 8 characters with complexity requirements
    if (password.length() < 8) {
        return false;
    }
    
    // Maximum reasonable length to prevent DoS
    if (password.length() > 128) {
        return false;
    }
    
    bool has_upper = false;
    bool has_lower = false;
    bool has_digit = false;
    bool has_special = false;
    
    for (char c : password) {
        if (std::isupper(static_cast<unsigned char>(c))) has_upper = true;
        else if (std::islower(static_cast<unsigned char>(c))) has_lower = true;
        else if (std::isdigit(static_cast<unsigned char>(c))) has_digit = true;
        else has_special = true;
    }
    
    // Require at least 3 of 4 character types
    int complexity = (has_upper ? 1 : 0) + (has_lower ? 1 : 0) + 
                     (has_digit ? 1 : 0) + (has_special ? 1 : 0);
    return complexity >= 3;
}

std::string AuthManager::generateRandomToken() {
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

} // namespace xipher
