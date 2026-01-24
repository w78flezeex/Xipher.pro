/**
 * Xipher Wallet API Handler
 * 
 * Handles wallet-related API endpoints:
 * - /api/wallet/register - Register wallet addresses for user
 * - /api/wallet/get-address - Get user's wallet address
 * - /api/wallet/balance - Get multi-chain balance (stub)
 * - /api/wallet/relay-tx - Broadcast signed transaction
 * - /api/wallet/send-to-user - Send crypto to user by username
 * - /api/wallet/history - Get transaction history
 * 
 * NOTE: Most wallet operations happen client-side for security.
 * The server only stores public addresses and transaction records.
 */

#include "../include/server/request_handler.hpp"
#include "../include/utils/json_parser.hpp"
#include "../include/utils/logger.hpp"
#include <curl/curl.h>
#include <sstream>
#include <iomanip>

namespace {

// Chain RPC endpoints
const std::map<std::string, std::string> CHAIN_RPC = {
    {"ethereum", "https://eth.llamarpc.com"},
    {"polygon", "https://polygon-rpc.com"},
    {"solana", "https://api.mainnet-beta.solana.com"},
    {"ton", "https://toncenter.com/api/v2"}
};

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string httpGet(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response;
    
    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "User-Agent: Xipher-Messenger/1.0");
        headers = curl_slist_append(headers, "Accept: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            // Ignore errors, return empty
        }
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    
    return response;
}

std::string httpPost(const std::string& url, const std::string& body, const std::string& contentType = "application/json") {
    CURL* curl = curl_easy_init();
    std::string response;
    
    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Content-Type: " + contentType).c_str());
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            // Ignore errors, return empty
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    
    return response;
}

} // anonymous namespace

namespace xipher {

/**
 * Register wallet addresses for a user
 * POST /api/wallet/register
 * Body: { token, addresses: { ethereum, polygon, solana, ton } }
 */
std::string RequestHandler::handleWalletRegister(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    // Get addresses from nested object
    std::string eth_addr, polygon_addr, sol_addr, ton_addr;
    
    if (data.count("addresses")) {
        auto addresses = JsonParser::parse(data["addresses"]);
        eth_addr = addresses["ethereum"];
        polygon_addr = addresses["polygon"];
        sol_addr = addresses["solana"];
        ton_addr = addresses["ton"];
    }
    
    // Store wallet addresses (using profile or creating a simple storage)
    // For now, we'll just return success - full implementation would store in DB
    
    std::ostringstream response;
    response << "{\"success\":true,\"message\":\"Wallet addresses registered\","
             << "\"user_id\":\"" << user_id << "\"}";
    
    return response.str();
}

/**
 * Get wallet address for a user (by username)
 * POST /api/wallet/get-address
 * Body: { token, username }
 */
std::string RequestHandler::handleWalletGetAddress(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    std::string username = data["username"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    if (username.empty()) {
        return JsonParser::createErrorResponse("Username required");
    }
    
    // Get target user
    auto user = db_manager_.getUserByUsername(username);
    if (user.id.empty()) {
        return JsonParser::createErrorResponse("User not found");
    }
    
    // For now return stub - full implementation would fetch from DB
    std::ostringstream response;
    response << "{\"success\":true,\"user_id\":\"" << user.id << "\","
             << "\"username\":\"" << username << "\","
             << "\"addresses\":{}}";
    
    return response.str();
}

/**
 * Get wallet balance (prices from CoinGecko, balances from client)
 * POST /api/wallet/balance
 * Body: { token }
 * 
 * NOTE: Actual balances are fetched client-side. Server just validates session.
 */
std::string RequestHandler::handleWalletBalance(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    // Fetch prices from CoinGecko (cached on client, this is backup)
    std::string priceUrl = "https://api.coingecko.com/api/v3/simple/price?ids=ethereum,matic-network,solana,the-open-network&vs_currencies=usd";
    std::string priceData = httpGet(priceUrl);
    
    std::ostringstream response;
    response << "{\"success\":true,\"prices\":" << (priceData.empty() ? "{}" : priceData) << "}";
    
    return response.str();
}

/**
 * Relay a signed transaction to the blockchain
 * POST /api/wallet/relay-tx
 * Body: { token, chain, signed_tx }
 */
std::string RequestHandler::handleWalletRelayTx(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    std::string chain = data["chain"];
    std::string signed_tx = data["signed_tx"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    if (chain.empty() || signed_tx.empty()) {
        return JsonParser::createErrorResponse("Chain and signed_tx required");
    }
    
    // Find RPC endpoint
    auto rpcIt = CHAIN_RPC.find(chain);
    if (rpcIt == CHAIN_RPC.end()) {
        return JsonParser::createErrorResponse("Unsupported chain");
    }
    
    std::string rpcUrl = rpcIt->second;
    std::string txHash;
    
    // Relay transaction based on chain
    if (chain == "ethereum" || chain == "polygon") {
        // EVM chains - JSON-RPC
        std::ostringstream rpcBody;
        rpcBody << "{\"jsonrpc\":\"2.0\",\"method\":\"eth_sendRawTransaction\","
                << "\"params\":[\"" << signed_tx << "\"],\"id\":1}";
        
        std::string result = httpPost(rpcUrl, rpcBody.str());
        
        // Parse result for txHash
        auto resultData = JsonParser::parse(result);
        if (resultData.count("result")) {
            txHash = resultData["result"];
        } else if (resultData.count("error")) {
            return JsonParser::createErrorResponse("Transaction failed");
        }
    } else if (chain == "solana") {
        // Solana JSON-RPC
        std::ostringstream rpcBody;
        rpcBody << "{\"jsonrpc\":\"2.0\",\"method\":\"sendTransaction\","
                << "\"params\":[\"" << signed_tx << "\",{\"encoding\":\"base64\"}],\"id\":1}";
        
        std::string result = httpPost(rpcUrl, rpcBody.str());
        auto resultData = JsonParser::parse(result);
        if (resultData.count("result")) {
            txHash = resultData["result"];
        }
    } else if (chain == "ton") {
        // TON API
        std::string tonUrl = rpcUrl + "/sendBoc";
        std::ostringstream tonBody;
        tonBody << "{\"boc\":\"" << signed_tx << "\"}";
        
        std::string result = httpPost(tonUrl, tonBody.str());
        auto resultData = JsonParser::parse(result);
        if (resultData.count("ok") && resultData["ok"] == "true") {
            txHash = resultData.count("result") ? resultData["result"] : "pending";
        }
    }
    
    if (txHash.empty()) {
        return JsonParser::createErrorResponse("Failed to broadcast transaction");
    }
    
    std::ostringstream response;
    response << "{\"success\":true,\"tx_hash\":\"" << txHash << "\",\"chain\":\"" << chain << "\"}";
    
    return response.str();
}

/**
 * Get transaction history for user
 * POST /api/wallet/history
 * Body: { token, limit? }
 */
std::string RequestHandler::handleWalletHistory(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    // Return empty array for now - history tracked client-side in localStorage
    std::ostringstream response;
    response << "{\"success\":true,\"transactions\":[]}";
    
    return response.str();
}

/**
 * Send crypto to another user by username
 * POST /api/wallet/send-to-user
 * Body: { token, recipient_username, chain, amount }
 * 
 * Returns recipient's address so client can build and sign transaction
 */
std::string RequestHandler::handleWalletSendToUser(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    std::string recipient_username = data["recipient_username"];
    std::string chain = data["chain"];
    std::string amount = data["amount"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    if (recipient_username.empty() || chain.empty() || amount.empty()) {
        return JsonParser::createErrorResponse("recipient_username, chain, and amount required");
    }
    
    // Get recipient user
    auto recipient = db_manager_.getUserByUsername(recipient_username);
    if (recipient.id.empty()) {
        return JsonParser::createErrorResponse("Recipient not found");
    }
    
    // For now, return recipient info - client handles the actual sending
    std::ostringstream response;
    response << "{\"success\":true,"
             << "\"recipient_id\":\"" << recipient.id << "\","
             << "\"recipient_username\":\"" << recipient_username << "\","
             << "\"chain\":\"" << chain << "\","
             << "\"amount\":\"" << amount << "\","
             << "\"message\":\"Use client-side wallet to complete transfer\"}";
    
    return response.str();
}

/**
 * Proxy for CoinGecko API (to avoid CORS)
 * GET /api/wallet/tokens?page=1&per_page=250
 */
std::string RequestHandler::handleWalletTokens(const std::string& query) {
    // Parse query params
    int page = 1;
    int per_page = 250;
    
    // Simple query parsing
    if (!query.empty()) {
        size_t pagePos = query.find("page=");
        if (pagePos != std::string::npos) {
            page = std::stoi(query.substr(pagePos + 5));
        }
        size_t perPagePos = query.find("per_page=");
        if (perPagePos != std::string::npos) {
            per_page = std::stoi(query.substr(perPagePos + 9));
        }
    }
    
    // Limit per_page
    if (per_page > 250) per_page = 250;
    if (per_page < 1) per_page = 10;
    if (page < 1) page = 1;
    
    std::ostringstream url;
    url << "https://api.coingecko.com/api/v3/coins/markets?"
        << "vs_currency=usd&order=market_cap_desc"
        << "&per_page=" << per_page
        << "&page=" << page
        << "&sparkline=false&price_change_percentage=24h";
    
    std::string response = httpGet(url.str());
    
    if (response.empty()) {
        return "[]";
    }
    
    return response;
}

/**
 * Proxy for CoinGecko prices API
 * GET /api/wallet/prices?ids=bitcoin,ethereum
 */
std::string RequestHandler::handleWalletPrices(const std::string& query) {
    std::string ids = "bitcoin,ethereum,solana,the-open-network,matic-network";
    
    // Parse ids from query
    if (!query.empty()) {
        size_t idsPos = query.find("ids=");
        if (idsPos != std::string::npos) {
            size_t endPos = query.find('&', idsPos);
            if (endPos == std::string::npos) {
                ids = query.substr(idsPos + 4);
            } else {
                ids = query.substr(idsPos + 4, endPos - idsPos - 4);
            }
        }
    }
    
    std::ostringstream url;
    url << "https://api.coingecko.com/api/v3/simple/price?"
        << "ids=" << ids
        << "&vs_currencies=usd&include_24hr_change=true";
    
    std::string response = httpGet(url.str());
    
    if (response.empty()) {
        return "{}";
    }
    
    return response;
}

/**
 * Save wallet to server (encrypted seed stored on server)
 * POST /api/wallet/save
 * Body: { token, encrypted_seed, salt, addresses }
 */
std::string RequestHandler::handleWalletSave(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    std::string encrypted_seed = data["encrypted_seed"];
    std::string salt = data["salt"];
    std::string addresses = data["addresses"];
    
    if (encrypted_seed.empty() || salt.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    // Sanitize inputs
    auto sanitize = [](const std::string& s) {
        std::string result;
        for (char c : s) {
            if (c == '\'') result += "''";
            else if (c == '\\') result += "\\\\";
            else result += c;
        }
        return result;
    };
    
    std::ostringstream sql;
    sql << "INSERT INTO user_wallets (user_id, encrypted_seed, salt, addresses) "
        << "VALUES ('" << user_id << "', "
        << "'" << sanitize(encrypted_seed) << "', "
        << "'" << sanitize(salt) << "', "
        << "'" << sanitize(addresses) << "'::jsonb) "
        << "ON CONFLICT (user_id) DO UPDATE SET "
        << "encrypted_seed = EXCLUDED.encrypted_seed, "
        << "salt = EXCLUDED.salt, "
        << "addresses = EXCLUDED.addresses, "
        << "updated_at = NOW() "
        << "RETURNING user_id";
    
    try {
        PGresult* result = db_manager_.getDb()->executeQuery(sql.str());
        bool success = (result && PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0);
        if (result) PQclear(result);
        
        if (!success) {
            return JsonParser::createErrorResponse("Failed to save wallet");
        }
        
        return "{\"status\":\"success\",\"message\":\"Wallet saved\"}";
    } catch (const std::exception& e) {
        Logger::getInstance().error("Wallet save error: " + std::string(e.what()));
        return JsonParser::createErrorResponse("Database error");
    }
}

/**
 * Get wallet from server
 * POST /api/wallet/get
 * Body: { token }
 */
std::string RequestHandler::handleWalletGet(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    std::ostringstream sql;
    sql << "SELECT encrypted_seed, salt, addresses, balances, user_tokens, "
        << "created_at, updated_at FROM user_wallets WHERE user_id = '" << user_id << "'";
    
    try {
        PGresult* result = db_manager_.getDb()->executeQuery(sql.str());
        
        if (!result || PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
            if (result) PQclear(result);
            // No wallet exists
            return "{\"status\":\"success\",\"data\":null}";
        }
        
        std::string encrypted_seed = PQgetvalue(result, 0, 0) ? PQgetvalue(result, 0, 0) : "";
        std::string salt = PQgetvalue(result, 0, 1) ? PQgetvalue(result, 0, 1) : "";
        std::string addresses = PQgetvalue(result, 0, 2) ? PQgetvalue(result, 0, 2) : "{}";
        std::string balances = PQgetvalue(result, 0, 3) ? PQgetvalue(result, 0, 3) : "{}";
        std::string user_tokens = PQgetvalue(result, 0, 4) ? PQgetvalue(result, 0, 4) : "[]";
        
        PQclear(result);
        
        std::ostringstream response;
        response << "{\"status\":\"success\",\"data\":{"
                 << "\"encrypted_seed\":\"" << encrypted_seed << "\","
                 << "\"salt\":\"" << salt << "\","
                 << "\"addresses\":\"" << addresses << "\","
                 << "\"balances\":" << balances << ","
                 << "\"user_tokens\":" << user_tokens << "}}";
        
        return response.str();
    } catch (const std::exception& e) {
        Logger::getInstance().error("Wallet get error: " + std::string(e.what()));
        return JsonParser::createErrorResponse("Database error");
    }
}

/**
 * Update wallet balances/tokens on server
 * POST /api/wallet/update
 * Body: { token, balances?, user_tokens? }
 */
std::string RequestHandler::handleWalletUpdate(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    std::string balances = data["balances"];
    std::string user_tokens = data["user_tokens"];
    
    auto sanitize = [](const std::string& s) {
        std::string result;
        for (char c : s) {
            if (c == '\'') result += "''";
            else if (c == '\\') result += "\\\\";
            else result += c;
        }
        return result;
    };
    
    std::ostringstream sql;
    sql << "UPDATE user_wallets SET updated_at = NOW()";
    
    if (!balances.empty()) {
        sql << ", balances = '" << sanitize(balances) << "'::jsonb";
    }
    if (!user_tokens.empty()) {
        sql << ", user_tokens = '" << sanitize(user_tokens) << "'::jsonb";
    }
    
    sql << " WHERE user_id = '" << user_id << "' RETURNING user_id";
    
    try {
        PGresult* result = db_manager_.getDb()->executeQuery(sql.str());
        bool success = (result && PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) > 0);
        if (result) PQclear(result);
        
        if (!success) {
            return JsonParser::createErrorResponse("Wallet not found");
        }
        
        return "{\"status\":\"success\",\"message\":\"Wallet updated\"}";
    } catch (const std::exception& e) {
        Logger::getInstance().error("Wallet update error: " + std::string(e.what()));
        return JsonParser::createErrorResponse("Database error");
    }
}

/**
 * Delete wallet from server
 * POST /api/wallet/delete
 * Body: { token }
 */
std::string RequestHandler::handleWalletDelete(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string session_token = data["token"];
    
    if (session_token.empty()) {
        return JsonParser::createErrorResponse("Token required");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(session_token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid or expired token");
    }
    
    std::ostringstream sql;
    sql << "DELETE FROM user_wallets WHERE user_id = '" << user_id << "'";
    
    try {
        PGresult* result = db_manager_.getDb()->executeQuery(sql.str());
        if (result) PQclear(result);
        return "{\"status\":\"success\",\"message\":\"Wallet deleted\"}";
    } catch (const std::exception& e) {
        Logger::getInstance().error("Wallet delete error: " + std::string(e.what()));
        return JsonParser::createErrorResponse("Database error");
    }
}

} // namespace xipher
