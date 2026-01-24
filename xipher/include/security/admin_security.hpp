#ifndef ADMIN_SECURITY_HPP
#define ADMIN_SECURITY_HPP

#include <string>
#include <unordered_map>
#include <chrono>

namespace xipher {

struct AdminSessionInfo {
    std::string token;
    std::string user_id;
    std::string username;
    std::string ip_hash;
    std::string user_agent_hash;
    std::chrono::steady_clock::time_point created_at;
    bool is_2fa_verified = false;
};

class RateLimiter {
public:
    RateLimiter(int max_attempts = 3, int window_seconds = 60, int ban_seconds = 86400);
    bool allow(const std::string& key, std::string& ban_reason);
    void clear(const std::string& key);

private:
    struct Entry {
        int attempts = 0;
        std::chrono::steady_clock::time_point first_attempt;
        std::chrono::steady_clock::time_point banned_until;
    };

    int max_attempts_;
    int window_seconds_;
    int ban_seconds_;
    std::unordered_map<std::string, Entry> entries_;
};

std::string Base32Decode(const std::string& input);
bool VerifyTOTP(const std::string& base32_secret, const std::string& code, int timestep_seconds = 30, int drift_windows = 1);

} // namespace xipher

#endif // ADMIN_SECURITY_HPP
