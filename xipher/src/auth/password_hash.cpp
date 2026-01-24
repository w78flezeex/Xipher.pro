#include "../include/auth/password_hash.hpp"
#include <argon2.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace xipher {

namespace {
constexpr uint32_t kArgon2TimeCost = 3;
constexpr uint32_t kArgon2MemoryKiB = 65536;
constexpr uint32_t kArgon2Parallelism = 1;
constexpr uint32_t kArgon2SaltLength = 16;
constexpr uint32_t kArgon2HashLength = 32;

uint32_t clampUint32(uint32_t value, uint32_t min_value, uint32_t max_value) {
    return std::max(min_value, std::min(value, max_value));
}

uint32_t readEnvUint(const char* name, uint32_t fallback, uint32_t min_value, uint32_t max_value) {
    const char* raw = std::getenv(name);
    if (!raw || !*raw) {
        return fallback;
    }
    try {
        uint32_t parsed = static_cast<uint32_t>(std::stoul(raw));
        return clampUint32(parsed, min_value, max_value);
    } catch (...) {
        return fallback;
    }
}

bool constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    }
    return diff == 0;
}
} // namespace

std::string PasswordHash::hash(const std::string& password) {
    const uint32_t t_cost = readEnvUint("XIPHER_ARGON2_TIME_COST", kArgon2TimeCost, 2, 10);
    const uint32_t m_cost = readEnvUint("XIPHER_ARGON2_MEMORY_KIB", kArgon2MemoryKiB, 32768, 524288);
    const uint32_t parallelism = readEnvUint("XIPHER_ARGON2_PARALLELISM", kArgon2Parallelism, 1, 8);

    unsigned char salt[kArgon2SaltLength];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        return "";
    }

    const size_t encoded_len = argon2_encodedlen(t_cost, m_cost, parallelism,
                                                 kArgon2SaltLength, kArgon2HashLength, Argon2_id);
    std::string encoded(encoded_len, '\0');

    const int result = argon2id_hash_encoded(
        t_cost,
        m_cost,
        parallelism,
        password.data(),
        password.size(),
        salt,
        sizeof(salt),
        kArgon2HashLength,
        &encoded[0],
        encoded.size());

    if (result != ARGON2_OK) {
        return "";
    }

    encoded.resize(std::strlen(encoded.c_str()));
    return encoded;
}

bool PasswordHash::verify(const std::string& password, const std::string& hash) {
    if (hash.empty()) {
        return false;
    }

    if (isArgon2Hash(hash)) {
        return argon2id_verify(hash.c_str(), password.data(), password.size()) == ARGON2_OK;
    }

    return verifyLegacySha256(password, hash);
}

bool PasswordHash::needsRehash(const std::string& hash) {
    return !isArgon2Hash(hash);
}

std::string PasswordHash::generateSalt() {
    unsigned char salt[16];
    if (RAND_bytes(salt, sizeof(salt)) != 1) {
        return "";
    }
    
    std::ostringstream salt_hex;
    for (int i = 0; i < 16; i++) {
        salt_hex << std::hex << std::setw(2) << std::setfill('0') << (int)salt[i];
    }
    return salt_hex.str();
}

bool PasswordHash::verifyLegacySha256(const std::string& password, const std::string& hash) {
    const size_t colon_pos = hash.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }

    const std::string salt = hash.substr(0, colon_pos);
    const std::string stored_hash = hash.substr(colon_pos + 1);

    unsigned char computed_hash[SHA256_DIGEST_LENGTH];
    const std::string salted_password = salt + password;

    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, salted_password.c_str(), salted_password.length());
    SHA256_Final(computed_hash, &sha256);

    std::ostringstream hash_hex;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hash_hex << std::hex << std::setw(2) << std::setfill('0') << (int)computed_hash[i];
    }

    return constantTimeEquals(hash_hex.str(), stored_hash);
}

bool PasswordHash::isArgon2Hash(const std::string& hash) {
    return hash.rfind("$argon2id$", 0) == 0;
}

} // namespace xipher
