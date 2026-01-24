#ifndef PASSWORD_HASH_HPP
#define PASSWORD_HASH_HPP

#include <string>

namespace xipher {

class PasswordHash {
public:
    // Generate password hash using Argon2id (OWASP recommended)
    static std::string hash(const std::string& password);
    
    // Verify password against hash
    static bool verify(const std::string& password, const std::string& hash);

    // Check whether stored hash should be upgraded (legacy format or old params)
    static bool needsRehash(const std::string& hash);
    
    // Generate random salt
    static std::string generateSalt();
    
private:
    static bool verifyLegacySha256(const std::string& password, const std::string& hash);
    static bool isArgon2Hash(const std::string& hash);
};

} // namespace xipher

#endif // PASSWORD_HASH_HPP
