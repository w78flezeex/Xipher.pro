/**
 * E2EE API Handlers
 * 
 * Provides API endpoints for:
 * - Key registration
 * - Key retrieval
 * - Encrypted message exchange
 * 
 * OWASP 2025: A04 - Cryptographic Failures - Prevention
 */

#include "../include/server/request_handler.hpp"
#include "../include/crypto/e2ee_manager.hpp"
#include "../include/utils/json_parser.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>

namespace xipher {

/**
 * POST /api/e2ee/register-key
 * Register user's public key for E2EE.
 * 
 * Request: { "token": "...", "public_key": "base64..." }
 * Response: { "success": true }
 */
std::string RequestHandler::handleE2EERegisterKey(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string public_key = data["public_key"];
    
    if (token.empty() || public_key.empty()) {
        return JsonParser::createErrorResponse("Token and public_key required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto& e2ee = crypto::E2EEManager::getInstance();
    if (e2ee.registerPublicKey(user_id, public_key)) {
        return JsonParser::createSuccessResponse("Public key registered");
    }
    
    return JsonParser::createErrorResponse("Failed to register public key");
}

/**
 * POST /api/e2ee/get-public-key
 * Get a user's public key for key exchange.
 * 
 * Request: { "token": "...", "user_id": "..." }
 * Response: { "success": true, "public_key": "base64...", "key_version": 1 }
 */
std::string RequestHandler::handleE2EEGetPublicKey(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string target_user_id = data["user_id"];
    
    if (token.empty() || target_user_id.empty()) {
        return JsonParser::createErrorResponse("Token and user_id required");
    }
    
    std::string requester_id = auth_manager_.getUserIdFromToken(token);
    if (requester_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Optional: Check if users are friends before sharing public key
    // For now, allow key retrieval for any authenticated user
    
    auto& e2ee = crypto::E2EEManager::getInstance();
    auto public_key = e2ee.getUserPublicKey(target_user_id);
    
    if (!public_key) {
        return JsonParser::createErrorResponse("User has no E2EE keys");
    }
    
    std::ostringstream oss;
    oss << "{\"success\":true,"
        << "\"public_key\":\"" << JsonParser::escapeJson(*public_key) << "\","
        << "\"key_version\":1}";
    
    return oss.str();
}

/**
 * POST /api/e2ee/get-public-keys
 * Get public keys for multiple users (batch, for group chats).
 * 
 * Request: { "token": "...", "user_ids": ["id1", "id2", ...] }
 * Response: { "success": true, "keys": { "id1": "key1", "id2": "key2" } }
 */
std::string RequestHandler::handleE2EEGetPublicKeys(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string requester_id = auth_manager_.getUserIdFromToken(token);
    if (requester_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Parse user_ids array
    std::vector<std::string> user_ids;
    // TODO: Parse JSON array from data["user_ids"]
    // For now, this is a simplified implementation
    
    auto& e2ee = crypto::E2EEManager::getInstance();
    auto keys = e2ee.getPublicKeys(user_ids);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"keys\":{";
    
    bool first = true;
    for (const auto& [user_id, key] : keys) {
        if (!first) oss << ",";
        oss << "\"" << JsonParser::escapeJson(user_id) << "\":\""
            << JsonParser::escapeJson(key) << "\"";
        first = false;
    }
    
    oss << "}}";
    return oss.str();
}

/**
 * POST /api/e2ee/send-encrypted
 * Send an encrypted message.
 * Server stores without decryption capability.
 * 
 * Request: {
 *   "token": "...",
 *   "receiver_id": "...",
 *   "encrypted_content": "base64...",
 *   "nonce": "base64...",
 *   "ephemeral_public_key": "base64..." (optional, for forward secrecy)
 * }
 */
std::string RequestHandler::handleE2EESendMessage(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string receiver_id = data["receiver_id"];
    std::string encrypted_content = data["encrypted_content"];
    std::string nonce = data["nonce"];
    std::string ephemeral_key = data.count("ephemeral_public_key") ? data["ephemeral_public_key"] : "";
    
    if (token.empty() || receiver_id.empty() || encrypted_content.empty() || nonce.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string sender_id = auth_manager_.getUserIdFromToken(token);
    if (sender_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Verify friendship (or saved messages)
    if (sender_id != receiver_id && !db_manager_.areFriends(sender_id, receiver_id)) {
        return JsonParser::createErrorResponse("Not friends");
    }
    
    // Store encrypted message
    // Server cannot decrypt - only stores for recipient
    auto& e2ee = crypto::E2EEManager::getInstance();
    
    // Get sender's public key for the message
    auto sender_key = e2ee.getUserPublicKey(sender_id);
    std::string sender_key_str = sender_key.value_or("");
    
    if (e2ee.storeEncryptedMessage(sender_id, receiver_id, encrypted_content, nonce, 
                                    ephemeral_key.empty() ? sender_key_str : ephemeral_key)) {
        // Also send via WebSocket if available
        if (ws_sender_) {
            std::ostringstream ws_payload;
            ws_payload << "{\"type\":\"e2ee_message\","
                       << "\"sender_id\":\"" << JsonParser::escapeJson(sender_id) << "\","
                       << "\"encrypted_content\":\"" << JsonParser::escapeJson(encrypted_content) << "\","
                       << "\"nonce\":\"" << JsonParser::escapeJson(nonce) << "\","
                       << "\"ephemeral_key\":\"" << JsonParser::escapeJson(ephemeral_key) << "\"}";
            ws_sender_(receiver_id, ws_payload.str());
        }
        
        return JsonParser::createSuccessResponse("Encrypted message sent");
    }
    
    return JsonParser::createErrorResponse("Failed to send encrypted message");
}

/**
 * POST /api/e2ee/status
 * Check E2EE status for a user or conversation.
 * 
 * Request: { "token": "...", "peer_id": "..." (optional) }
 * Response: { "success": true, "e2ee_enabled": true, "key_version": 1 }
 */
std::string RequestHandler::handleE2EEStatus(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string peer_id = data.count("peer_id") ? data["peer_id"] : "";
    
    if (token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    auto& e2ee = crypto::E2EEManager::getInstance();
    
    bool user_has_keys = e2ee.hasUserKeys(user_id);
    bool peer_has_keys = peer_id.empty() ? false : e2ee.hasUserKeys(peer_id);
    bool e2ee_enabled = user_has_keys && (peer_id.empty() || peer_has_keys);
    
    std::ostringstream oss;
    oss << "{\"success\":true,"
        << "\"e2ee_enabled\":" << (e2ee_enabled ? "true" : "false") << ","
        << "\"user_has_keys\":" << (user_has_keys ? "true" : "false") << ","
        << "\"peer_has_keys\":" << (peer_has_keys ? "true" : "false") << "}";
    
    return oss.str();
}

} // namespace xipher
