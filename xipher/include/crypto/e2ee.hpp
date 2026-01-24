#pragma once

/**
 * Xipher End-to-End Encryption Module
 * 
 * Implements modern E2EE using:
 * - X25519 for key exchange
 * - AES-256-GCM for message encryption
 * - HKDF for key derivation
 * 
 * Based on Signal Protocol principles but simplified.
 * For full implementation, consider using libsignal-protocol-c.
 * 
 * OWASP 2025: A04 - Cryptographic Failures
 */

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

namespace xipher {
namespace crypto {

/**
 * Key pair for X25519 key exchange
 */
struct KeyPair {
    std::vector<uint8_t> public_key;  // 32 bytes
    std::vector<uint8_t> private_key; // 32 bytes
};

/**
 * Encrypted message envelope
 */
struct EncryptedMessage {
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> nonce;      // 12 bytes for AES-GCM
    std::vector<uint8_t> tag;        // 16 bytes authentication tag
    std::string sender_public_key;   // Base64 encoded
    uint64_t message_id;
    uint64_t timestamp;
};

/**
 * E2EE Crypto Provider
 * 
 * Provides cryptographic operations for end-to-end encryption.
 * All operations use constant-time implementations where possible.
 */
class CryptoProvider {
public:
    CryptoProvider();
    ~CryptoProvider();
    
    /**
     * Generate a new X25519 key pair for a user.
     * Should be called once when user registers.
     */
    static KeyPair generateKeyPair();
    
    /**
     * Derive a shared secret from own private key and peer's public key.
     * Uses X25519 ECDH.
     */
    static std::vector<uint8_t> deriveSharedSecret(
        const std::vector<uint8_t>& own_private_key,
        const std::vector<uint8_t>& peer_public_key
    );
    
    /**
     * Derive encryption keys from shared secret using HKDF.
     * Returns a 32-byte key for AES-256.
     */
    static std::vector<uint8_t> deriveMessageKey(
        const std::vector<uint8_t>& shared_secret,
        const std::string& context
    );
    
    /**
     * Encrypt a message using AES-256-GCM.
     * 
     * @param plaintext The message to encrypt
     * @param key 32-byte encryption key
     * @param associated_data Additional authenticated data (e.g., sender ID)
     * @return Encrypted message envelope
     */
    static EncryptedMessage encryptMessage(
        const std::string& plaintext,
        const std::vector<uint8_t>& key,
        const std::string& associated_data = ""
    );
    
    /**
     * Decrypt a message using AES-256-GCM.
     * 
     * @param encrypted The encrypted message envelope
     * @param key 32-byte decryption key
     * @param associated_data Additional authenticated data
     * @return Decrypted plaintext, or empty string if decryption fails
     */
    static std::string decryptMessage(
        const EncryptedMessage& encrypted,
        const std::vector<uint8_t>& key,
        const std::string& associated_data = ""
    );
    
    /**
     * Generate cryptographically secure random bytes.
     */
    static std::vector<uint8_t> randomBytes(size_t length);
    
    /**
     * Base64 encode bytes.
     */
    static std::string base64Encode(const std::vector<uint8_t>& data);
    
    /**
     * Base64 decode string to bytes.
     */
    static std::vector<uint8_t> base64Decode(const std::string& encoded);
    
    /**
     * Compute SHA-256 hash.
     */
    static std::vector<uint8_t> sha256(const std::string& data);
    
    /**
     * Compute HMAC-SHA256.
     */
    static std::vector<uint8_t> hmacSha256(
        const std::vector<uint8_t>& key,
        const std::string& data
    );
    
    /**
     * Constant-time comparison of two byte arrays.
     */
    static bool constantTimeEquals(
        const std::vector<uint8_t>& a,
        const std::vector<uint8_t>& b
    );

private:
    // Prevent copying
    CryptoProvider(const CryptoProvider&) = delete;
    CryptoProvider& operator=(const CryptoProvider&) = delete;
};

/**
 * Simple Ratchet for Forward Secrecy
 * 
 * Implements a simplified version of the Double Ratchet algorithm.
 * Each message uses a new key derived from the previous one.
 */
class MessageRatchet {
public:
    MessageRatchet(const std::vector<uint8_t>& initial_chain_key);
    
    /**
     * Get the next message key and advance the ratchet.
     */
    std::vector<uint8_t> nextMessageKey();
    
    /**
     * Get the current chain key (for serialization).
     */
    std::vector<uint8_t> getChainKey() const;
    
private:
    std::vector<uint8_t> chain_key_;
    uint32_t message_number_;
};

} // namespace crypto
} // namespace xipher
