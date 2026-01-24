#include "../../include/security/admin_security.hpp"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <ctime>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cctype>

namespace xipher {

RateLimiter::RateLimiter(int max_attempts, int window_seconds, int ban_seconds)
    : max_attempts_(max_attempts),
      window_seconds_(window_seconds),
      ban_seconds_(ban_seconds) {}

bool RateLimiter::allow(const std::string& key, std::string& ban_reason) {
    using clock = std::chrono::steady_clock;
    const auto now = clock::now();

    auto& entry = entries_[key];

    if (entry.banned_until.time_since_epoch().count() > 0 && now < entry.banned_until) {
        ban_reason = "temporarily banned";
        return false;
    }

    if (entry.attempts == 0 || (now - entry.first_attempt) > std::chrono::seconds(window_seconds_)) {
        entry.attempts = 0;
        entry.first_attempt = now;
    }

    entry.attempts++;

    if (entry.attempts > max_attempts_) {
        entry.banned_until = now + std::chrono::seconds(ban_seconds_);
        ban_reason = "too many attempts";
        return false;
    }

    ban_reason.clear();
    return true;
}

void RateLimiter::clear(const std::string& key) {
    entries_.erase(key);
}

static int base32Value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '2' && c <= '7') return 26 + (c - '2');
    return -1;
}

std::string Base32Decode(const std::string& input) {
    std::vector<unsigned char> bytes;
    int buffer = 0;
    int bits_left = 0;

    for (char raw_c : input) {
        if (raw_c == '=') break;
        char c = static_cast<char>(std::toupper(raw_c));
        int val = base32Value(c);
        if (val < 0) continue;

        buffer <<= 5;
        buffer |= val;
        bits_left += 5;

        if (bits_left >= 8) {
            bits_left -= 8;
            bytes.push_back(static_cast<unsigned char>((buffer >> bits_left) & 0xFF));
        }
    }

    return std::string(bytes.begin(), bytes.end());
}

static std::string hmacSha1(const std::string& key, uint64_t counter) {
    unsigned char counter_bytes[8];
    for (int i = 7; i >= 0; --i) {
        counter_bytes[i] = static_cast<unsigned char>(counter & 0xFF);
        counter >>= 8;
    }

    unsigned int len = 0;
    unsigned char result[EVP_MAX_MD_SIZE];

    HMAC(EVP_sha1(),
         reinterpret_cast<const unsigned char*>(key.data()), static_cast<int>(key.size()),
         counter_bytes, sizeof(counter_bytes),
         result, &len);

    return std::string(reinterpret_cast<char*>(result), len);
}

bool VerifyTOTP(const std::string& base32_secret, const std::string& code, int timestep_seconds, int drift_windows) {
    if (code.size() != 6 || base32_secret.empty()) {
        return false;
    }

    const std::string key = Base32Decode(base32_secret);
    if (key.empty()) {
        return false;
    }

    const uint64_t timestep = static_cast<uint64_t>(timestep_seconds);
    const uint64_t current_counter = static_cast<uint64_t>(std::time(nullptr)) / timestep;

    for (int i = -drift_windows; i <= drift_windows; ++i) {
        uint64_t counter = current_counter + i;
        const std::string hmac = hmacSha1(key, counter);
        const int offset = hmac[hmac.size() - 1] & 0x0F;
        uint32_t binary = (static_cast<unsigned char>(hmac[offset]) & 0x7F) << 24 |
                          (static_cast<unsigned char>(hmac[offset + 1]) & 0xFF) << 16 |
                          (static_cast<unsigned char>(hmac[offset + 2]) & 0xFF) << 8 |
                          (static_cast<unsigned char>(hmac[offset + 3]) & 0xFF);
        uint32_t otp = binary % 1000000;

        std::ostringstream oss;
        oss << std::setw(6) << std::setfill('0') << otp;
        if (oss.str() == code) {
            return true;
        }
    }

    return false;
}

} // namespace xipher
