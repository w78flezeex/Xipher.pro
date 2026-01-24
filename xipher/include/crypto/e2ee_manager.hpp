#pragma once

/**
 * E2EE Key Management and Message Encryption Manager
 * 
 * Handles:
 * - User key pair generation and storage
 * - Key exchange between users
 * - Message encryption/decryption
 * - Key rotation
 * 
 * OWASP 2025: A04 - Cryptographic Failures - Prevention
 */

#include "e2ee.hpp"
#include "../database/db_manager.hpp"
#include <string>
#include <optional>
#include <unordered_map>
#include <mutex>

namespace xipher {
namespace crypto {

/**
 * User's E2EE key material
 */
struct UserKeys {
    std::string user_id;
    std::string public_key_b64;      // Base64 encoded X25519 public key
    std::string private_key_enc_b64; // Base64 encoded encrypted private key
    std::string created_at;
    uint32_t key_version;
};

/**
 * Session keys for a conversation (sender, receiver pair)
 */
struct SessionKeys {
    std::string session_id;
    std::vector<uint8_t> shared_secret;
    std::vector<uint8_t> send_chain_key;
    std::vector<uint8_t> recv_chain_key;
    uint32_t send_counter;
    uint32_t recv_counter;
};

/**
 * E2EE Manager - Singleton for managing encryption across the server
 * 
 * NOTE: In true E2EE, the server should NOT have access to private keys.
 * This implementation stores encrypted private keys that only clients can decrypt.
 * Server assists with key distribution but cannot decrypt messages.
 */
class E2EEManager {
public:
    static E2EEManager& getInstance();
    
    /**
     * Generate and store a new key pair for a user.
     * Private key is encrypted with user's password-derived key before storage.
     * 
     * @param user_id User's ID
     * @param password_key Key derived from user's password (client-side)
     * @return Public key in Base64 format
     */
    std::string generateUserKeyPair(const std::string& user_id);
    
    /**
     * Get user's public key for key exchange.
     */
    std::optional<std::string> getUserPublicKey(const std::string& user_id);
    
    /**
     * Store encrypted message.
     * Server stores ciphertext without ability to decrypt.
     */
    bool storeEncryptedMessage(
        const std::string& sender_id,
        const std::string& receiver_id,
        const std::string& encrypted_content_b64,
        const std::string& nonce_b64,
        const std::string& sender_public_key_b64
    );
    
    /**
     * Get encrypted messages for a user.
     * Returns still-encrypted messages for client-side decryption.
     */
    std::vector<std::string> getEncryptedMessages(
        const std::string& user_id,
        const std::string& peer_id,
        int limit = 50
    );
    
    /**
     * Check if user has E2EE keys registered.
     */
    bool hasUserKeys(const std::string& user_id);
    
    /**
     * Register user's public key (uploaded from client).
     * In true E2EE, client generates keys and sends only public key.
     */
    bool registerPublicKey(
        const std::string& user_id,
        const std::string& public_key_b64,
        uint32_t key_version = 1
    );
    
    /**
     * Get all public keys for a list of users (for group chats).
     */
    std::unordered_map<std::string, std::string> getPublicKeys(
        const std::vector<std::string>& user_ids
    );

private:
    E2EEManager() = default;
    ~E2EEManager() = default;
    
    // Prevent copying
    E2EEManager(const E2EEManager&) = delete;
    E2EEManager& operator=(const E2EEManager&) = delete;
    
    // In-memory cache of public keys
    std::unordered_map<std::string, std::string> public_key_cache_;
    std::mutex cache_mutex_;
    
    // Database reference (set during initialization)
    DatabaseManager* db_ = nullptr;
    
public:
    void setDatabase(DatabaseManager* db) { db_ = db; }
};

} // namespace crypto
} // namespace xipher
