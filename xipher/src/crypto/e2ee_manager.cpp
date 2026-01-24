/**
 * E2EE Manager Implementation
 * 
 * Server-side component for E2EE key management.
 * NOTE: Server only stores public keys and encrypted messages.
 * Private keys exist only on client devices.
 */

#include "crypto/e2ee_manager.hpp"
#include "crypto/e2ee.hpp"
#include "utils/logger.hpp"
#include <sstream>

namespace xipher {
namespace crypto {

E2EEManager& E2EEManager::getInstance() {
    static E2EEManager instance;
    return instance;
}

std::string E2EEManager::generateUserKeyPair(const std::string& user_id) {
    try {
        // Generate X25519 key pair
        auto key_pair = CryptoProvider::generateKeyPair();
        
        // Encode public key as Base64
        std::string public_key_b64 = CryptoProvider::base64Encode(key_pair.public_key);
        
        // In true E2EE, private key should NEVER be stored on server
        // It should be generated and stored on client device only
        // We only store public key on server for key exchange
        
        // Store public key in database
        if (db_) {
            registerPublicKey(user_id, public_key_b64, 1);
        }
        
        // Cache it
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            public_key_cache_[user_id] = public_key_b64;
        }
        
        Logger::getInstance().info("E2EE: Generated key pair for user " + user_id);
        
        // Return public key for client to store
        // Private key should be returned to client securely (over TLS)
        // and never stored on server
        return public_key_b64;
        
    } catch (const std::exception& e) {
        Logger::getInstance().error("E2EE: Failed to generate key pair: " + std::string(e.what()));
        return "";
    }
}

std::optional<std::string> E2EEManager::getUserPublicKey(const std::string& user_id) {
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = public_key_cache_.find(user_id);
        if (it != public_key_cache_.end()) {
            return it->second;
        }
    }
    
    // Query database
    // TODO: Add e2ee_keys table query
    // For now return empty
    return std::nullopt;
}

bool E2EEManager::hasUserKeys(const std::string& user_id) {
    return getUserPublicKey(user_id).has_value();
}

bool E2EEManager::registerPublicKey(
    const std::string& user_id,
    const std::string& public_key_b64,
    uint32_t key_version
) {
    // Validate public key format (should be 44 chars for 32 bytes base64)
    if (public_key_b64.length() < 40 || public_key_b64.length() > 48) {
        Logger::getInstance().warning("E2EE: Invalid public key length for user " + user_id);
        return false;
    }
    
    // Try to decode to validate
    try {
        auto decoded = CryptoProvider::base64Decode(public_key_b64);
        if (decoded.size() != 32) {
            Logger::getInstance().warning("E2EE: Invalid decoded key size for user " + user_id);
            return false;
        }
    } catch (const std::exception& e) {
        Logger::getInstance().warning("E2EE: Failed to decode public key for user " + user_id);
        return false;
    }
    
    // Cache it
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        public_key_cache_[user_id] = public_key_b64;
    }
    
    // TODO: Store in database (e2ee_keys table)
    // db_->storeUserPublicKey(user_id, public_key_b64, key_version);
    
    Logger::getInstance().info("E2EE: Registered public key for user " + user_id + " (v" + std::to_string(key_version) + ")");
    return true;
}

std::unordered_map<std::string, std::string> E2EEManager::getPublicKeys(
    const std::vector<std::string>& user_ids
) {
    std::unordered_map<std::string, std::string> result;
    
    for (const auto& user_id : user_ids) {
        auto key = getUserPublicKey(user_id);
        if (key) {
            result[user_id] = *key;
        }
    }
    
    return result;
}

bool E2EEManager::storeEncryptedMessage(
    const std::string& sender_id,
    const std::string& receiver_id,
    const std::string& encrypted_content_b64,
    const std::string& nonce_b64,
    const std::string& sender_public_key_b64
) {
    // Server stores encrypted message without ability to decrypt
    // This is the essence of E2EE - server is just a relay
    
    // TODO: Store in messages table with encryption metadata
    // For now, log and return success
    Logger::getInstance().debug("E2EE: Storing encrypted message from " + sender_id + " to " + receiver_id);
    return true;
}

std::vector<std::string> E2EEManager::getEncryptedMessages(
    const std::string& user_id,
    const std::string& peer_id,
    int limit
) {
    std::vector<std::string> messages;
    
    // TODO: Query messages table for encrypted messages
    // Return as-is for client decryption
    
    return messages;
}

} // namespace crypto
} // namespace xipher
