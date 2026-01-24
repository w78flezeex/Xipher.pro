/**
 * Xipher End-to-End Encryption Implementation
 * 
 * Uses OpenSSL for cryptographic primitives:
 * - X25519 for key exchange
 * - AES-256-GCM for authenticated encryption
 * - HKDF for key derivation
 * 
 * OWASP 2025: A04 - Cryptographic Failures - Fix
 */

#include "crypto/e2ee.hpp"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <cstring>
#include <stdexcept>
#include <chrono>

namespace xipher {
namespace crypto {

// Constants
static constexpr size_t KEY_SIZE = 32;
static constexpr size_t NONCE_SIZE = 12;
static constexpr size_t TAG_SIZE = 16;
static constexpr size_t X25519_KEY_SIZE = 32;

// RAII wrapper for EVP_PKEY
class EvpPkeyPtr {
public:
    explicit EvpPkeyPtr(EVP_PKEY* ptr = nullptr) : ptr_(ptr) {}
    ~EvpPkeyPtr() { if (ptr_) EVP_PKEY_free(ptr_); }
    EVP_PKEY* get() const { return ptr_; }
    EVP_PKEY* release() { auto p = ptr_; ptr_ = nullptr; return p; }
    EvpPkeyPtr(const EvpPkeyPtr&) = delete;
    EvpPkeyPtr& operator=(const EvpPkeyPtr&) = delete;
private:
    EVP_PKEY* ptr_;
};

// RAII wrapper for EVP_PKEY_CTX
class EvpPkeyCtxPtr {
public:
    explicit EvpPkeyCtxPtr(EVP_PKEY_CTX* ptr = nullptr) : ptr_(ptr) {}
    ~EvpPkeyCtxPtr() { if (ptr_) EVP_PKEY_CTX_free(ptr_); }
    EVP_PKEY_CTX* get() const { return ptr_; }
    EvpPkeyCtxPtr(const EvpPkeyCtxPtr&) = delete;
    EvpPkeyCtxPtr& operator=(const EvpPkeyCtxPtr&) = delete;
private:
    EVP_PKEY_CTX* ptr_;
};

// RAII wrapper for EVP_CIPHER_CTX
class EvpCipherCtxPtr {
public:
    EvpCipherCtxPtr() : ptr_(EVP_CIPHER_CTX_new()) {}
    ~EvpCipherCtxPtr() { if (ptr_) EVP_CIPHER_CTX_free(ptr_); }
    EVP_CIPHER_CTX* get() const { return ptr_; }
    EvpCipherCtxPtr(const EvpCipherCtxPtr&) = delete;
    EvpCipherCtxPtr& operator=(const EvpCipherCtxPtr&) = delete;
private:
    EVP_CIPHER_CTX* ptr_;
};

CryptoProvider::CryptoProvider() = default;
CryptoProvider::~CryptoProvider() = default;

std::vector<uint8_t> CryptoProvider::randomBytes(size_t length) {
    std::vector<uint8_t> buffer(length);
    if (RAND_bytes(buffer.data(), static_cast<int>(length)) != 1) {
        throw std::runtime_error("Failed to generate random bytes");
    }
    return buffer;
}

KeyPair CryptoProvider::generateKeyPair() {
    KeyPair kp;
    
    // Generate X25519 key pair
    EvpPkeyCtxPtr pctx(EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr));
    if (!pctx.get()) {
        throw std::runtime_error("Failed to create key context");
    }
    
    if (EVP_PKEY_keygen_init(pctx.get()) <= 0) {
        throw std::runtime_error("Failed to init keygen");
    }
    
    EVP_PKEY* raw_pkey = nullptr;
    if (EVP_PKEY_keygen(pctx.get(), &raw_pkey) <= 0) {
        throw std::runtime_error("Failed to generate key pair");
    }
    EvpPkeyPtr pkey(raw_pkey);
    
    // Extract public key
    size_t pub_len = X25519_KEY_SIZE;
    kp.public_key.resize(pub_len);
    if (EVP_PKEY_get_raw_public_key(pkey.get(), kp.public_key.data(), &pub_len) != 1) {
        throw std::runtime_error("Failed to extract public key");
    }
    
    // Extract private key
    size_t priv_len = X25519_KEY_SIZE;
    kp.private_key.resize(priv_len);
    if (EVP_PKEY_get_raw_private_key(pkey.get(), kp.private_key.data(), &priv_len) != 1) {
        throw std::runtime_error("Failed to extract private key");
    }
    
    return kp;
}

std::vector<uint8_t> CryptoProvider::deriveSharedSecret(
    const std::vector<uint8_t>& own_private_key,
    const std::vector<uint8_t>& peer_public_key
) {
    if (own_private_key.size() != X25519_KEY_SIZE || peer_public_key.size() != X25519_KEY_SIZE) {
        throw std::invalid_argument("Invalid key size");
    }
    
    // Create EVP_PKEY from raw keys
    EvpPkeyPtr own_key(EVP_PKEY_new_raw_private_key(
        EVP_PKEY_X25519, nullptr,
        own_private_key.data(), own_private_key.size()
    ));
    if (!own_key.get()) {
        throw std::runtime_error("Failed to create private key");
    }
    
    EvpPkeyPtr peer_key(EVP_PKEY_new_raw_public_key(
        EVP_PKEY_X25519, nullptr,
        peer_public_key.data(), peer_public_key.size()
    ));
    if (!peer_key.get()) {
        throw std::runtime_error("Failed to create peer public key");
    }
    
    // Perform key exchange
    EvpPkeyCtxPtr ctx(EVP_PKEY_CTX_new(own_key.get(), nullptr));
    if (!ctx.get()) {
        throw std::runtime_error("Failed to create derivation context");
    }
    
    if (EVP_PKEY_derive_init(ctx.get()) <= 0) {
        throw std::runtime_error("Failed to init derivation");
    }
    
    if (EVP_PKEY_derive_set_peer(ctx.get(), peer_key.get()) <= 0) {
        throw std::runtime_error("Failed to set peer key");
    }
    
    // Get shared secret length
    size_t secret_len = 0;
    if (EVP_PKEY_derive(ctx.get(), nullptr, &secret_len) <= 0) {
        throw std::runtime_error("Failed to get secret length");
    }
    
    std::vector<uint8_t> shared_secret(secret_len);
    if (EVP_PKEY_derive(ctx.get(), shared_secret.data(), &secret_len) <= 0) {
        throw std::runtime_error("Failed to derive shared secret");
    }
    
    return shared_secret;
}

std::vector<uint8_t> CryptoProvider::deriveMessageKey(
    const std::vector<uint8_t>& shared_secret,
    const std::string& context
) {
    std::vector<uint8_t> key(KEY_SIZE);
    
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!pctx) {
        throw std::runtime_error("Failed to create HKDF context");
    }
    
    if (EVP_PKEY_derive_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to init HKDF");
    }
    
    if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF hash");
    }
    
    // Salt (use context or empty)
    std::vector<uint8_t> salt(16, 0);
    if (EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt.data(), salt.size()) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF salt");
    }
    
    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, shared_secret.data(), shared_secret.size()) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF key");
    }
    
    // Info (context string)
    const uint8_t* info = reinterpret_cast<const uint8_t*>(context.c_str());
    if (EVP_PKEY_CTX_add1_hkdf_info(pctx, info, context.size()) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to set HKDF info");
    }
    
    size_t key_len = KEY_SIZE;
    if (EVP_PKEY_derive(pctx, key.data(), &key_len) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        throw std::runtime_error("Failed to derive key");
    }
    
    EVP_PKEY_CTX_free(pctx);
    return key;
}

EncryptedMessage CryptoProvider::encryptMessage(
    const std::string& plaintext,
    const std::vector<uint8_t>& key,
    const std::string& associated_data
) {
    if (key.size() != KEY_SIZE) {
        throw std::invalid_argument("Invalid key size");
    }
    
    EncryptedMessage result;
    result.nonce = randomBytes(NONCE_SIZE);
    result.message_id = 0; // Set by caller
    result.timestamp = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count()
    );
    
    EvpCipherCtxPtr ctx;
    if (!ctx.get()) {
        throw std::runtime_error("Failed to create cipher context");
    }
    
    if (EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        throw std::runtime_error("Failed to init encryption");
    }
    
    // Set nonce length
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, NONCE_SIZE, nullptr) != 1) {
        throw std::runtime_error("Failed to set nonce length");
    }
    
    // Set key and nonce
    if (EVP_EncryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), result.nonce.data()) != 1) {
        throw std::runtime_error("Failed to set key/nonce");
    }
    
    // Add AAD if provided
    int len = 0;
    if (!associated_data.empty()) {
        if (EVP_EncryptUpdate(ctx.get(), nullptr, &len,
                reinterpret_cast<const uint8_t*>(associated_data.c_str()),
                static_cast<int>(associated_data.size())) != 1) {
            throw std::runtime_error("Failed to add AAD");
        }
    }
    
    // Encrypt
    result.ciphertext.resize(plaintext.size() + 16); // Extra space for padding
    if (EVP_EncryptUpdate(ctx.get(), result.ciphertext.data(), &len,
            reinterpret_cast<const uint8_t*>(plaintext.c_str()),
            static_cast<int>(plaintext.size())) != 1) {
        throw std::runtime_error("Failed to encrypt");
    }
    int ciphertext_len = len;
    
    // Finalize
    if (EVP_EncryptFinal_ex(ctx.get(), result.ciphertext.data() + len, &len) != 1) {
        throw std::runtime_error("Failed to finalize encryption");
    }
    ciphertext_len += len;
    result.ciphertext.resize(ciphertext_len);
    
    // Get tag
    result.tag.resize(TAG_SIZE);
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_GET_TAG, TAG_SIZE, result.tag.data()) != 1) {
        throw std::runtime_error("Failed to get tag");
    }
    
    return result;
}

std::string CryptoProvider::decryptMessage(
    const EncryptedMessage& encrypted,
    const std::vector<uint8_t>& key,
    const std::string& associated_data
) {
    if (key.size() != KEY_SIZE) {
        throw std::invalid_argument("Invalid key size");
    }
    if (encrypted.nonce.size() != NONCE_SIZE) {
        throw std::invalid_argument("Invalid nonce size");
    }
    if (encrypted.tag.size() != TAG_SIZE) {
        throw std::invalid_argument("Invalid tag size");
    }
    
    EvpCipherCtxPtr ctx;
    if (!ctx.get()) {
        throw std::runtime_error("Failed to create cipher context");
    }
    
    if (EVP_DecryptInit_ex(ctx.get(), EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        throw std::runtime_error("Failed to init decryption");
    }
    
    // Set nonce length
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_IVLEN, NONCE_SIZE, nullptr) != 1) {
        throw std::runtime_error("Failed to set nonce length");
    }
    
    // Set key and nonce
    if (EVP_DecryptInit_ex(ctx.get(), nullptr, nullptr, key.data(), encrypted.nonce.data()) != 1) {
        throw std::runtime_error("Failed to set key/nonce");
    }
    
    // Add AAD if provided
    int len = 0;
    if (!associated_data.empty()) {
        if (EVP_DecryptUpdate(ctx.get(), nullptr, &len,
                reinterpret_cast<const uint8_t*>(associated_data.c_str()),
                static_cast<int>(associated_data.size())) != 1) {
            throw std::runtime_error("Failed to add AAD");
        }
    }
    
    // Decrypt
    std::vector<uint8_t> plaintext(encrypted.ciphertext.size());
    if (EVP_DecryptUpdate(ctx.get(), plaintext.data(), &len,
            encrypted.ciphertext.data(),
            static_cast<int>(encrypted.ciphertext.size())) != 1) {
        return ""; // Decryption failed - likely tampered
    }
    int plaintext_len = len;
    
    // Set expected tag
    if (EVP_CIPHER_CTX_ctrl(ctx.get(), EVP_CTRL_GCM_SET_TAG, TAG_SIZE,
            const_cast<uint8_t*>(encrypted.tag.data())) != 1) {
        return ""; // Tag verification setup failed
    }
    
    // Finalize and verify tag
    if (EVP_DecryptFinal_ex(ctx.get(), plaintext.data() + len, &len) != 1) {
        return ""; // Tag verification failed - message tampered
    }
    plaintext_len += len;
    
    return std::string(reinterpret_cast<char*>(plaintext.data()), plaintext_len);
}

std::string CryptoProvider::base64Encode(const std::vector<uint8_t>& data) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, data.data(), static_cast<int>(data.size()));
    BIO_flush(bio);
    
    BUF_MEM* buf_mem = nullptr;
    BIO_get_mem_ptr(bio, &buf_mem);
    
    std::string result(buf_mem->data, buf_mem->length);
    BIO_free_all(bio);
    
    return result;
}

std::vector<uint8_t> CryptoProvider::base64Decode(const std::string& encoded) {
    BIO* bio = BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
    BIO* b64 = BIO_new(BIO_f_base64());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    
    std::vector<uint8_t> result(encoded.size());
    int decoded_len = BIO_read(bio, result.data(), static_cast<int>(result.size()));
    
    BIO_free_all(bio);
    
    if (decoded_len < 0) {
        throw std::runtime_error("Base64 decode failed");
    }
    
    result.resize(decoded_len);
    return result;
}

std::vector<uint8_t> CryptoProvider::sha256(const std::string& data) {
    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
    SHA256(reinterpret_cast<const uint8_t*>(data.c_str()), data.size(), hash.data());
    return hash;
}

std::vector<uint8_t> CryptoProvider::hmacSha256(
    const std::vector<uint8_t>& key,
    const std::string& data
) {
    std::vector<uint8_t> result(SHA256_DIGEST_LENGTH);
    unsigned int result_len = 0;
    
    HMAC(EVP_sha256(),
         key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const uint8_t*>(data.c_str()), data.size(),
         result.data(), &result_len);
    
    result.resize(result_len);
    return result;
}

bool CryptoProvider::constantTimeEquals(
    const std::vector<uint8_t>& a,
    const std::vector<uint8_t>& b
) {
    if (a.size() != b.size()) {
        return false;
    }
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

// MessageRatchet implementation
MessageRatchet::MessageRatchet(const std::vector<uint8_t>& initial_chain_key)
    : chain_key_(initial_chain_key), message_number_(0)
{
    if (chain_key_.size() != KEY_SIZE) {
        throw std::invalid_argument("Invalid chain key size");
    }
}

std::vector<uint8_t> MessageRatchet::nextMessageKey() {
    // Derive message key from chain key
    std::vector<uint8_t> message_key = CryptoProvider::hmacSha256(
        chain_key_,
        "message_key_" + std::to_string(message_number_)
    );
    
    // Advance chain key (forward secrecy)
    chain_key_ = CryptoProvider::hmacSha256(chain_key_, "chain_advance");
    message_number_++;
    
    return message_key;
}

std::vector<uint8_t> MessageRatchet::getChainKey() const {
    return chain_key_;
}

} // namespace crypto
} // namespace xipher
