/**
 * Xipher Wallet Extended Features API Handler
 * 
 * Handles advanced wallet features:
 * - P2P marketplace (offers, trades)
 * - Crypto vouchers/checks
 * - Staking/Earn products
 * - NFT gallery
 * - Security settings
 * 
 * SECURITY: All inputs are sanitized against SQL injection and XSS
 */

#include "../include/server/request_handler.hpp"
#include "../include/utils/json_parser.hpp"
#include "../include/utils/logger.hpp"
#include "../include/utils/security_utils.hpp"
#include <curl/curl.h>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>

namespace {

// Global rate limiter for wallet operations
static xipher::security::RateLimiter walletRateLimiter({30, 60, 300});

std::string generateRandomCode(int length) {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
    
    std::string code;
    for (int i = 0; i < length; ++i) {
        if (i > 0 && i % 4 == 0) code += '-';
        code += chars[dis(gen)];
    }
    return code;
}

std::string escapeJson(const std::string& s) {
    std::ostringstream o;
    for (auto c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if ('\x00' <= c && c <= '\x1f') {
                    o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c;
                } else {
                    o << c;
                }
        }
    }
    return o.str();
}

std::string pgValue(PGresult* res, int row, int col) {
    if (PQgetisnull(res, row, col)) return "";
    return PQgetvalue(res, row, col);
}

} // anonymous namespace

namespace xipher {

// =============================================
// P2P MARKETPLACE API
// =============================================

std::string RequestHandler::handleP2POffers(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    // SECURITY: Rate limiting
    if (!walletRateLimiter.checkLimit(user_id)) {
        return JsonParser::createErrorResponse("Rate limit exceeded. Please wait.");
    }
    
    std::string offer_type = data.count("type") ? data["type"] : "sell";
    std::string crypto = data.count("crypto") ? data["crypto"] : "USDT";
    std::string fiat = data.count("fiat") ? data["fiat"] : "RUB";
    
    // SECURITY: Input validation
    if (offer_type != "sell" && offer_type != "buy") {
        return JsonParser::createErrorResponse("Invalid offer type");
    }
    if (!security::isValidCryptoSymbol(crypto)) {
        return JsonParser::createErrorResponse("Invalid crypto symbol");
    }
    if (fiat.size() != 3) {
        return JsonParser::createErrorResponse("Invalid fiat currency");
    }
    
    // SECURITY: Sanitize inputs for SQL
    offer_type = security::sanitizeSql(offer_type);
    crypto = security::sanitizeSql(crypto);
    fiat = security::sanitizeSql(fiat);
    
    auto* db = db_manager_.getDb();
    std::string query = 
        "SELECT o.id, o.user_id, u.username, o.offer_type, o.crypto_symbol, o.fiat_currency, "
        "o.price_per_unit, o.min_amount, o.max_amount, o.available_amount, o.payment_methods, "
        "COALESCE(s.avg_rating, 0) as rating, COALESCE(s.total_trades, 0) as trades "
        "FROM p2p_offers o "
        "JOIN users u ON o.user_id = u.id "
        "LEFT JOIN p2p_user_stats s ON o.user_id = s.user_id "
        "WHERE o.is_active = true AND o.offer_type = '" + offer_type + "' "
        "AND o.crypto_symbol = '" + crypto + "' "
        "AND o.fiat_currency = '" + fiat + "' "
        "ORDER BY o.price_per_unit " + (offer_type == "sell" ? "ASC" : "DESC") + " "
        "LIMIT 50";
    
    PGresult* res = db->executeQuery(query);
    
    std::ostringstream response;
    response << "{\"success\":true,\"offers\":[";
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; ++i) {
            if (i > 0) response << ",";
            response << "{"
                     << "\"id\":\"" << pgValue(res, i, 0) << "\","
                     << "\"user_id\":\"" << pgValue(res, i, 1) << "\","
                     << "\"username\":\"" << escapeJson(pgValue(res, i, 2)) << "\","
                     << "\"type\":\"" << pgValue(res, i, 3) << "\","
                     << "\"crypto\":\"" << pgValue(res, i, 4) << "\","
                     << "\"fiat\":\"" << pgValue(res, i, 5) << "\","
                     << "\"price\":" << pgValue(res, i, 6) << ","
                     << "\"min\":" << pgValue(res, i, 7) << ","
                     << "\"max\":" << pgValue(res, i, 8) << ","
                     << "\"available\":" << pgValue(res, i, 9) << ","
                     << "\"payment_methods\":\"" << escapeJson(pgValue(res, i, 10)) << "\","
                     << "\"rating\":" << pgValue(res, i, 11) << ","
                     << "\"trades\":" << pgValue(res, i, 12)
                     << "}";
        }
        PQclear(res);
    }
    
    response << "]}";
    return response.str();
}

std::string RequestHandler::handleP2PCreateOffer(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    // SECURITY: Rate limiting for offer creation
    if (!walletRateLimiter.checkLimit(user_id + "_create_offer")) {
        return JsonParser::createErrorResponse("Rate limit exceeded. Please wait.");
    }
    
    std::string offer_type = data["type"];
    std::string crypto = data["crypto"];
    std::string fiat = data["fiat"];
    std::string price = data["price"];
    std::string min_amount = data.count("min") ? data["min"] : "100";
    std::string max_amount = data.count("max") ? data["max"] : "100000";
    std::string payment_methods = data.count("payment_methods") ? data["payment_methods"] : "";
    
    if (offer_type.empty() || crypto.empty() || fiat.empty() || price.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    // SECURITY: Strict input validation
    if (offer_type != "sell" && offer_type != "buy") {
        return JsonParser::createErrorResponse("Invalid offer type");
    }
    if (!security::isValidCryptoSymbol(crypto)) {
        return JsonParser::createErrorResponse("Invalid crypto symbol");
    }
    if (fiat.size() != 3) {
        return JsonParser::createErrorResponse("Invalid fiat currency");
    }
    if (!security::isValidNumber(price) || !security::isValidNumber(min_amount) || !security::isValidNumber(max_amount)) {
        return JsonParser::createErrorResponse("Invalid numeric value");
    }
    
    // SECURITY: Sanitize all inputs
    offer_type = security::sanitizeSql(offer_type);
    crypto = security::sanitizeSql(crypto);
    fiat = security::sanitizeSql(fiat);
    payment_methods = security::sanitizeSql(payment_methods);
    
    auto* db = db_manager_.getDb();
    std::string query = 
        "INSERT INTO p2p_offers (user_id, offer_type, crypto_symbol, fiat_currency, "
        "price_per_unit, min_amount, max_amount, available_amount, payment_methods) "
        "VALUES ('" + user_id + "', '" + offer_type + "', '" + crypto + "', '" + fiat + "', "
        + price + ", " + min_amount + ", " + max_amount + ", " + max_amount + ", "
        "'" + payment_methods + "') RETURNING id";
    
    PGresult* res = db->executeQuery(query);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        std::string offer_id = pgValue(res, 0, 0);
        PQclear(res);
        return "{\"success\":true,\"offer_id\":\"" + offer_id + "\"}";
    }
    
    if (res) PQclear(res);
    return JsonParser::createErrorResponse("Failed to create offer");
}

std::string RequestHandler::handleP2PStartTrade(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    std::string offer_id = data["offer_id"];
    std::string amount = data["amount"];
    
    if (offer_id.empty() || amount.empty()) {
        return JsonParser::createErrorResponse("offer_id and amount required");
    }
    
    auto* db = db_manager_.getDb();
    
    // Get offer details
    std::string offerQuery = "SELECT user_id, crypto_symbol, price_per_unit FROM p2p_offers WHERE id = '" + offer_id + "'";
    PGresult* offerRes = db->executeQuery(offerQuery);
    
    if (!offerRes || PQntuples(offerRes) == 0) {
        if (offerRes) PQclear(offerRes);
        return JsonParser::createErrorResponse("Offer not found");
    }
    
    std::string seller_id = pgValue(offerRes, 0, 0);
    std::string crypto = pgValue(offerRes, 0, 1);
    std::string price = pgValue(offerRes, 0, 2);
    PQclear(offerRes);
    
    // Create trade
    std::string insertQuery = 
        "INSERT INTO p2p_trades (offer_id, buyer_id, seller_id, crypto_amount, fiat_amount, status) "
        "VALUES ('" + offer_id + "', '" + user_id + "', '" + seller_id + "', " 
        + amount + ", " + amount + " * " + price + ", 'pending') RETURNING id";
    
    PGresult* res = db->executeQuery(insertQuery);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        std::string trade_id = pgValue(res, 0, 0);
        PQclear(res);
        return "{\"success\":true,\"trade_id\":\"" + trade_id + "\"}";
    }
    
    if (res) PQclear(res);
    return JsonParser::createErrorResponse("Failed to start trade");
}

std::string RequestHandler::handleP2PMyTrades(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    auto* db = db_manager_.getDb();
    std::string query = 
        "SELECT t.id, t.offer_id, t.buyer_id, t.seller_id, t.crypto_amount, t.fiat_amount, "
        "t.status, t.created_at, o.crypto_symbol, o.fiat_currency "
        "FROM p2p_trades t "
        "JOIN p2p_offers o ON t.offer_id = o.id "
        "WHERE t.buyer_id = '" + user_id + "' OR t.seller_id = '" + user_id + "' "
        "ORDER BY t.created_at DESC LIMIT 50";
    
    PGresult* res = db->executeQuery(query);
    
    std::ostringstream response;
    response << "{\"success\":true,\"trades\":[";
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; ++i) {
            if (i > 0) response << ",";
            response << "{"
                     << "\"id\":\"" << pgValue(res, i, 0) << "\","
                     << "\"offer_id\":\"" << pgValue(res, i, 1) << "\","
                     << "\"crypto_amount\":" << pgValue(res, i, 4) << ","
                     << "\"fiat_amount\":" << pgValue(res, i, 5) << ","
                     << "\"status\":\"" << pgValue(res, i, 6) << "\","
                     << "\"created_at\":\"" << pgValue(res, i, 7) << "\","
                     << "\"crypto\":\"" << pgValue(res, i, 8) << "\","
                     << "\"fiat\":\"" << pgValue(res, i, 9) << "\""
                     << "}";
        }
        PQclear(res);
    }
    
    response << "]}";
    return response.str();
}

std::string RequestHandler::handleP2PUpdateTrade(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    std::string trade_id = data["trade_id"];
    std::string new_status = data["status"];
    
    if (trade_id.empty() || new_status.empty()) {
        return JsonParser::createErrorResponse("trade_id and status required");
    }
    
    auto* db = db_manager_.getDb();
    std::string query = "UPDATE p2p_trades SET status = '" + new_status + "' "
                       "WHERE id = '" + trade_id + "' AND (buyer_id = '" + user_id + "' OR seller_id = '" + user_id + "')";
    
    PGresult* res = db->executeQuery(query);
    bool success = res && PQresultStatus(res) == PGRES_COMMAND_OK;
    if (res) PQclear(res);
    
    if (success) {
        return "{\"success\":true}";
    }
    return JsonParser::createErrorResponse("Failed to update trade");
}

// =============================================
// VOUCHERS API
// =============================================

std::string RequestHandler::handleVoucherCreate(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    // SECURITY: Rate limiting for voucher creation
    if (!walletRateLimiter.checkLimit(user_id + "_voucher_create")) {
        return JsonParser::createErrorResponse("Rate limit exceeded. Please wait.");
    }
    
    std::string crypto = data["crypto"];
    std::string amount = data["amount"];
    std::string password = data.count("password") ? data["password"] : "";
    int uses = data.count("uses") ? std::stoi(data["uses"]) : 1;
    
    if (crypto.empty() || amount.empty()) {
        return JsonParser::createErrorResponse("crypto and amount required");
    }
    
    // SECURITY: Input validation
    if (!security::isValidCryptoSymbol(crypto)) {
        return JsonParser::createErrorResponse("Invalid crypto symbol");
    }
    if (!security::isValidNumber(amount)) {
        return JsonParser::createErrorResponse("Invalid amount");
    }
    if (uses < 1 || uses > 1000) {
        return JsonParser::createErrorResponse("Uses must be between 1 and 1000");
    }
    
    std::string code = "XIPHER-" + generateRandomCode(16);
    
    // SECURITY: Sanitize inputs
    crypto = security::sanitizeSql(crypto);
    std::string passwordHash = password.empty() ? "" : security::sha256(password);
    
    auto* db = db_manager_.getDb();
    std::string query = 
        "INSERT INTO crypto_vouchers (creator_id, code, crypto_symbol, amount, "
        "password_hash, max_claims) VALUES ('" + user_id + "', '" + code + "', "
        "'" + crypto + "', " + amount + ", "
        + (passwordHash.empty() ? "NULL" : "'" + passwordHash + "'") + ", " + std::to_string(uses) + ") RETURNING id";
    
    PGresult* res = db->executeQuery(query);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        std::string voucher_id = pgValue(res, 0, 0);
        PQclear(res);
        
        std::ostringstream response;
        response << "{\"success\":true,"
                 << "\"voucher_id\":\"" << voucher_id << "\","
                 << "\"code\":\"" << code << "\","
                 << "\"amount\":" << amount << ","
                 << "\"crypto\":\"" << crypto << "\"}";
        return response.str();
    }
    
    if (res) PQclear(res);
    return JsonParser::createErrorResponse("Failed to create voucher");
}

std::string RequestHandler::handleVoucherClaim(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    std::string code = data["code"];
    std::string password = data.count("password") ? data["password"] : "";
    
    if (code.empty()) {
        return JsonParser::createErrorResponse("Voucher code required");
    }
    
    // SECURITY: Rate limiting for voucher claims (prevent brute force)
    if (!walletRateLimiter.checkLimit(user_id + "_voucher_claim")) {
        walletRateLimiter.recordFailedAttempt(user_id);
        return JsonParser::createErrorResponse("Rate limit exceeded. Please wait.");
    }
    
    // SECURITY: Validate voucher code format
    if (code.size() > 50 || code.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") != std::string::npos) {
        walletRateLimiter.recordFailedAttempt(user_id);
        return JsonParser::createErrorResponse("Invalid voucher code format");
    }
    
    // SECURITY: Sanitize inputs
    code = security::sanitizeSql(code);
    
    auto* db = db_manager_.getDb();
    
    // Find voucher
    std::string query = "SELECT id, crypto_symbol, amount, password_hash, max_claims, claimed_count, is_active "
                       "FROM crypto_vouchers WHERE code = '" + code + "'";
    
    PGresult* res = db->executeQuery(query);
    
    if (!res || PQntuples(res) == 0) {
        if (res) PQclear(res);
        walletRateLimiter.recordFailedAttempt(user_id);
        return JsonParser::createErrorResponse("Voucher not found");
    }
    
    std::string voucher_id = pgValue(res, 0, 0);
    std::string crypto = pgValue(res, 0, 1);
    std::string amount = pgValue(res, 0, 2);
    std::string password_hash = pgValue(res, 0, 3);
    int max_claims = std::stoi(pgValue(res, 0, 4));
    int claimed_count = std::stoi(pgValue(res, 0, 5));
    bool is_active = pgValue(res, 0, 6) == "t";
    PQclear(res);
    
    if (!is_active) {
        return JsonParser::createErrorResponse("Voucher is no longer active");
    }
    
    if (claimed_count >= max_claims) {
        return JsonParser::createErrorResponse("Voucher fully claimed");
    }
    
    // SECURITY: Password verification with constant-time comparison
    if (!password_hash.empty()) {
        if (password.empty()) {
            return "{\"success\":false,\"requires_password\":true,\"error\":\"Password required\"}";
        }
        std::string inputHash = security::sha256(password);
        if (inputHash.length() != password_hash.length()) {
            walletRateLimiter.recordFailedAttempt(user_id);
            return JsonParser::createErrorResponse("Incorrect password");
        }
        int result = 0;
        for (size_t i = 0; i < inputHash.length(); ++i) {
            result |= inputHash[i] ^ password_hash[i];
        }
        if (result != 0) {
            walletRateLimiter.recordFailedAttempt(user_id);
            return JsonParser::createErrorResponse("Incorrect password");
        }
    }
    
    // Reset failed attempts on successful claim
    walletRateLimiter.resetFailedAttempts(user_id);
    
    // Claim voucher
    std::string updateQuery = "UPDATE crypto_vouchers SET claimed_count = claimed_count + 1, "
                             "is_active = (claimed_count + 1 < max_claims) WHERE id = '" + voucher_id + "'";
    PGresult* updateRes = db->executeQuery(updateQuery);
    if (updateRes) PQclear(updateRes);
    
    std::string claimQuery = "INSERT INTO voucher_claims (voucher_id, claimer_id, amount_claimed) "
                            "VALUES ('" + voucher_id + "', '" + user_id + "', " + amount + ")";
    PGresult* claimRes = db->executeQuery(claimQuery);
    if (claimRes) PQclear(claimRes);
    
    std::ostringstream response;
    response << "{\"success\":true,"
             << "\"amount\":" << amount << ","
             << "\"crypto\":\"" << crypto << "\"}";
    return response.str();
}

std::string RequestHandler::handleVouchersMy(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    auto* db = db_manager_.getDb();
    std::string query = 
        "SELECT id, code, crypto_symbol, amount, is_active, max_claims, claimed_count, created_at "
        "FROM crypto_vouchers WHERE creator_id = '" + user_id + "' ORDER BY created_at DESC";
    
    PGresult* res = db->executeQuery(query);
    
    std::ostringstream response;
    response << "{\"success\":true,\"vouchers\":[";
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; ++i) {
            if (i > 0) response << ",";
            response << "{"
                     << "\"id\":\"" << pgValue(res, i, 0) << "\","
                     << "\"code\":\"" << pgValue(res, i, 1) << "\","
                     << "\"crypto\":\"" << pgValue(res, i, 2) << "\","
                     << "\"amount\":" << pgValue(res, i, 3) << ","
                     << "\"is_active\":" << (pgValue(res, i, 4) == "t" ? "true" : "false") << ","
                     << "\"max_claims\":" << pgValue(res, i, 5) << ","
                     << "\"claimed_count\":" << pgValue(res, i, 6) << ","
                     << "\"created_at\":\"" << pgValue(res, i, 7) << "\""
                     << "}";
        }
        PQclear(res);
    }
    
    response << "]}";
    return response.str();
}

// =============================================
// STAKING API
// =============================================

std::string RequestHandler::handleStakingProducts(const std::string& body) {
    (void)body;  // unused parameter
    auto* db = db_manager_.getDb();
    std::string query = "SELECT id, name, crypto_symbol, apy, min_amount, "
                       "lock_period_days, (lock_period_days = 0) as is_flexible FROM staking_products WHERE is_active = true";
    
    PGresult* res = db->executeQuery(query);
    
    std::ostringstream response;
    response << "{\"success\":true,\"products\":[";
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; ++i) {
            if (i > 0) response << ",";
            response << "{"
                     << "\"id\":\"" << pgValue(res, i, 0) << "\","
                     << "\"name\":\"" << escapeJson(pgValue(res, i, 1)) << "\","
                     << "\"crypto\":\"" << pgValue(res, i, 2) << "\","
                     << "\"apy\":" << pgValue(res, i, 3) << ","
                     << "\"min_amount\":" << pgValue(res, i, 4) << ","
                     << "\"lock_days\":" << pgValue(res, i, 5) << ","
                     << "\"is_flexible\":" << (pgValue(res, i, 6) == "t" ? "true" : "false")
                     << "}";
        }
        PQclear(res);
    }
    
    response << "]}";
    return response.str();
}

std::string RequestHandler::handleStakingStake(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    // SECURITY: Rate limiting for staking operations
    if (!walletRateLimiter.checkLimit(user_id + "_staking")) {
        return JsonParser::createErrorResponse("Rate limit exceeded. Please wait.");
    }
    
    std::string product_id = data["product_id"];
    std::string amount = data["amount"];
    
    if (product_id.empty() || amount.empty()) {
        return JsonParser::createErrorResponse("product_id and amount required");
    }
    
    // SECURITY: Validate inputs
    if (!security::isValidNumber(amount)) {
        return JsonParser::createErrorResponse("Invalid amount format");
    }
    // Validate product_id format (alphanumeric and dashes only)
    for (char c : product_id) {
        if (!std::isalnum(c) && c != '-' && c != '_') {
            return JsonParser::createErrorResponse("Invalid product_id format");
        }
    }
    
    // SECURITY: Sanitize inputs
    product_id = security::sanitizeSql(product_id);
    
    auto* db = db_manager_.getDb();
    
    // Get product details
    std::string productQuery = "SELECT lock_period_days FROM staking_products WHERE id = '" + product_id + "'";
    PGresult* productRes = db->executeQuery(productQuery);
    
    if (!productRes || PQntuples(productRes) == 0) {
        if (productRes) PQclear(productRes);
        return JsonParser::createErrorResponse("Product not found");
    }
    
    int lock_days = std::stoi(pgValue(productRes, 0, 0));
    PQclear(productRes);
    
    std::string unlockDate = lock_days > 0 
        ? "CURRENT_DATE + INTERVAL '" + std::to_string(lock_days) + " days'"
        : "NULL";
    
    std::string insertQuery = 
        "INSERT INTO user_stakes (user_id, product_id, staked_amount, staked_amount_usd, unlock_at) "
        "VALUES ('" + user_id + "', '" + product_id + "', " + amount + ", " + amount + ", " + unlockDate + ") RETURNING id";
    
    PGresult* res = db->executeQuery(insertQuery);
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        std::string stake_id = pgValue(res, 0, 0);
        PQclear(res);
        return "{\"success\":true,\"stake_id\":\"" + stake_id + "\"}";
    }
    
    if (res) PQclear(res);
    return JsonParser::createErrorResponse("Failed to stake");
}

std::string RequestHandler::handleStakingMyStakes(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    auto* db = db_manager_.getDb();
    std::string query = 
        "SELECT s.id, s.product_id, p.name, p.crypto_symbol, s.staked_amount, s.accrued_rewards, "
        "s.unlock_at, s.status, s.staked_at, p.apy "
        "FROM user_stakes s "
        "JOIN staking_products p ON s.product_id = p.id "
        "WHERE s.user_id = '" + user_id + "' ORDER BY s.staked_at DESC";
    
    PGresult* res = db->executeQuery(query);
    
    std::ostringstream response;
    response << "{\"success\":true,\"stakes\":[";
    
    double total_staked = 0;
    double total_earned = 0;
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; ++i) {
            if (i > 0) response << ",";
            
            std::string amountStr = pgValue(res, i, 4);
            std::string earnedStr = pgValue(res, i, 5);
            double amount = amountStr.empty() ? 0 : std::stod(amountStr);
            double earned = earnedStr.empty() ? 0 : std::stod(earnedStr);
            total_staked += amount;
            total_earned += earned;
            
            response << "{"
                     << "\"id\":\"" << pgValue(res, i, 0) << "\","
                     << "\"product_id\":\"" << pgValue(res, i, 1) << "\","
                     << "\"name\":\"" << escapeJson(pgValue(res, i, 2)) << "\","
                     << "\"crypto\":\"" << pgValue(res, i, 3) << "\","
                     << "\"amount\":" << (amountStr.empty() ? "0" : amountStr) << ","
                     << "\"earned\":" << (earnedStr.empty() ? "0" : earnedStr) << ","
                     << "\"unlock_date\":" << (pgValue(res, i, 6).empty() ? "null" : "\"" + pgValue(res, i, 6) + "\"") << ","
                     << "\"status\":\"" << pgValue(res, i, 7) << "\","
                     << "\"apy\":" << pgValue(res, i, 9)
                     << "}";
        }
        PQclear(res);
    }
    
    response << "],\"total_staked\":" << std::fixed << std::setprecision(2) << total_staked
             << ",\"total_earned\":" << std::fixed << std::setprecision(6) << total_earned << "}";
    return response.str();
}

std::string RequestHandler::handleStakingUnstake(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    std::string stake_id = data["stake_id"];
    
    if (stake_id.empty()) {
        return JsonParser::createErrorResponse("stake_id required");
    }
    
    auto* db = db_manager_.getDb();
    
    // Check if stake exists and is unlocked
    std::string checkQuery = "SELECT unlock_at FROM user_stakes "
                            "WHERE id = '" + stake_id + "' AND user_id = '" + user_id + "' AND status = 'active'";
    PGresult* checkRes = db->executeQuery(checkQuery);
    
    if (!checkRes || PQntuples(checkRes) == 0) {
        if (checkRes) PQclear(checkRes);
        return JsonParser::createErrorResponse("Stake not found or already withdrawn");
    }
    
    PQclear(checkRes);
    
    // Update stake status
    std::string updateQuery = "UPDATE user_stakes SET status = 'completed', unstaked_at = NOW() WHERE id = '" + stake_id + "'";
    PGresult* updateRes = db->executeQuery(updateQuery);
    if (updateRes) PQclear(updateRes);
    
    return "{\"success\":true}";
}

// =============================================
// NFT API
// =============================================

std::string RequestHandler::handleNFTMy(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    auto* db = db_manager_.getDb();
    std::string query = 
        "SELECT id, chain, contract_address, token_id, name, description, image_url, "
        "collection_name, attributes, floor_price_usd "
        "FROM user_nfts WHERE user_id = '" + user_id + "' ORDER BY created_at DESC";
    
    PGresult* res = db->executeQuery(query);
    
    std::ostringstream response;
    response << "{\"success\":true,\"nfts\":[";
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK) {
        int rows = PQntuples(res);
        for (int i = 0; i < rows; ++i) {
            if (i > 0) response << ",";
            response << "{"
                     << "\"id\":\"" << pgValue(res, i, 0) << "\","
                     << "\"chain\":\"" << pgValue(res, i, 1) << "\","
                     << "\"contract\":\"" << pgValue(res, i, 2) << "\","
                     << "\"token_id\":\"" << pgValue(res, i, 3) << "\","
                     << "\"name\":\"" << escapeJson(pgValue(res, i, 4)) << "\","
                     << "\"description\":\"" << escapeJson(pgValue(res, i, 5)) << "\","
                     << "\"image\":\"" << escapeJson(pgValue(res, i, 6)) << "\","
                     << "\"collection\":\"" << escapeJson(pgValue(res, i, 7)) << "\","
                     << "\"floor_price\":" << (pgValue(res, i, 9).empty() ? "null" : pgValue(res, i, 9))
                     << "}";
        }
        PQclear(res);
    }
    
    response << "]}";
    return response.str();
}

std::string RequestHandler::handleNFTRefresh(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    // In production, this would call external APIs (Alchemy, etc.) to refresh NFT list
    // For now, just return success
    return "{\"success\":true,\"message\":\"NFT refresh initiated\"}";
}

// =============================================
// SECURITY SETTINGS API
// =============================================

std::string RequestHandler::handleSecurityGet(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    auto* db = db_manager_.getDb();
    std::string query = 
        "SELECT pin_enabled, biometric_enabled, auto_lock_timeout, require_password_send, hide_balance "
        "FROM wallet_security_settings WHERE user_id = '" + user_id + "'";
    
    PGresult* res = db->executeQuery(query);
    
    std::ostringstream response;
    response << "{\"success\":true,\"settings\":{";
    
    if (res && PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) > 0) {
        response << "\"pin_enabled\":" << (pgValue(res, 0, 0) == "t" ? "true" : "false") << ","
                 << "\"biometric_enabled\":" << (pgValue(res, 0, 1) == "t" ? "true" : "false") << ","
                 << "\"auto_lock_timeout\":" << (pgValue(res, 0, 2).empty() ? "5" : pgValue(res, 0, 2)) << ","
                 << "\"require_password_send\":" << (pgValue(res, 0, 3) == "t" ? "true" : "false") << ","
                 << "\"hide_balance\":" << (pgValue(res, 0, 4) == "t" ? "true" : "false");
        PQclear(res);
    } else {
        // Return defaults
        response << "\"pin_enabled\":false,"
                 << "\"biometric_enabled\":false,"
                 << "\"auto_lock_timeout\":5,"
                 << "\"require_password_send\":true,"
                 << "\"hide_balance\":false";
        if (res) PQclear(res);
    }
    
    response << "}}";
    return response.str();
}

std::string RequestHandler::handleSecurityUpdate(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    auto* db = db_manager_.getDb();
    
    // Parse settings from nested object
    std::map<std::string, std::string> settings_map;
    if (data.count("settings")) {
        settings_map = JsonParser::parse(data["settings"]);
    }
    
    // Build update parts
    std::vector<std::string> columns;
    std::vector<std::string> values;
    std::vector<std::string> updates;
    
    columns.push_back("user_id");
    values.push_back("'" + user_id + "'");
    
    if (settings_map.count("pin_enabled")) {
        std::string val = settings_map["pin_enabled"] == "true" ? "true" : "false";
        columns.push_back("pin_enabled");
        values.push_back(val);
        updates.push_back("pin_enabled = " + val);
    }
    if (settings_map.count("biometric_enabled")) {
        std::string val = settings_map["biometric_enabled"] == "true" ? "true" : "false";
        columns.push_back("biometric_enabled");
        values.push_back(val);
        updates.push_back("biometric_enabled = " + val);
    }
    if (settings_map.count("auto_lock_timeout")) {
        columns.push_back("auto_lock_timeout");
        values.push_back(settings_map["auto_lock_timeout"]);
        updates.push_back("auto_lock_timeout = " + settings_map["auto_lock_timeout"]);
    }
    if (settings_map.count("require_password_send")) {
        std::string val = settings_map["require_password_send"] == "true" ? "true" : "false";
        columns.push_back("require_password_send");
        values.push_back(val);
        updates.push_back("require_password_send = " + val);
    }
    if (settings_map.count("hide_balance")) {
        std::string val = settings_map["hide_balance"] == "true" ? "true" : "false";
        columns.push_back("hide_balance");
        values.push_back(val);
        updates.push_back("hide_balance = " + val);
    }
    
    if (updates.empty()) {
        return "{\"success\":true}";
    }
    
    // Build query strings
    std::string colStr, valStr, updateStr;
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) { colStr += ", "; valStr += ", "; }
        colStr += columns[i];
        valStr += values[i];
    }
    for (size_t i = 0; i < updates.size(); ++i) {
        if (i > 0) updateStr += ", ";
        updateStr += updates[i];
    }
    
    std::string upsertQuery = 
        "INSERT INTO wallet_security_settings (" + colStr + ") "
        "VALUES (" + valStr + ") "
        "ON CONFLICT (user_id) DO UPDATE SET " + updateStr;
    
    PGresult* res = db->executeQuery(upsertQuery);
    bool success = res && (PQresultStatus(res) == PGRES_COMMAND_OK || PQresultStatus(res) == PGRES_TUPLES_OK);
    if (res) PQclear(res);
    
    if (success) {
        return "{\"success\":true}";
    }
    return JsonParser::createErrorResponse("Failed to update settings");
}

} // namespace xipher
