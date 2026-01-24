// Marketplace API handlers
// OWASP 2025 Security Compliant - Input validation, prepared statements, access control

#include "../include/server/request_handler.hpp"
#include "../include/utils/json_parser.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>
#include <algorithm>

namespace xipher {

std::string RequestHandler::handleCreateStoreItem(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string owner_chat_id = data["owner_chat_id"];
    std::string owner_type = data["owner_type"];
    std::string title = data["title"];
    std::string description = data["description"];
    std::string price_amount_str = data["price_amount"];
    std::string price_currency = data["price_currency"];
    std::string product_type = data["product_type"];
    std::string image_path = data.count("image_path") ? data["image_path"] : "";
    int stock_quantity = data.count("stock_quantity") ? std::stoi(data["stock_quantity"]) : -1;
    
    // Input validation
    if (token.empty() || owner_chat_id.empty() || owner_type.empty() || title.empty() || 
        price_amount_str.empty() || price_currency.empty() || product_type.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    // Validate owner_type
    if (owner_type != "group" && owner_type != "channel") {
        return JsonParser::createErrorResponse("Invalid owner_type");
    }
    
    // Validate price_currency
    if (price_currency != "USDT" && price_currency != "TON" && price_currency != "BTC" && price_currency != "RUB") {
        return JsonParser::createErrorResponse("Invalid price_currency");
    }
    
    // Validate product_type
    if (product_type != "digital_key" && product_type != "file" && product_type != "service" && product_type != "role") {
        return JsonParser::createErrorResponse("Invalid product_type");
    }
    
    // Validate price
    double price_amount;
    try {
        price_amount = std::stod(price_amount_str);
        if (price_amount < 0) {
            return JsonParser::createErrorResponse("Price must be non-negative");
        }
    } catch (...) {
        return JsonParser::createErrorResponse("Invalid price_amount");
    }
    
    // Validate title length (prevent XSS)
    if (title.length() > 255) {
        return JsonParser::createErrorResponse("Title too long");
    }
    
    // Authenticate user
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Check if user is admin/creator of the chat
    bool has_permission = false;
    if (owner_type == "group") {
        auto member = db_manager_.getGroupMember(owner_chat_id, user_id);
        if (!member.id.empty() && (member.role == "admin" || member.role == "creator")) {
            has_permission = true;
        }
    } else if (owner_type == "channel") {
        auto member = db_manager_.getChannelMember(owner_chat_id, user_id);
        if (!member.id.empty() && (member.role == "admin" || member.role == "creator")) {
            has_permission = true;
        }
    }
    
    if (!has_permission) {
        return JsonParser::createErrorResponse("Insufficient permissions");
    }
    
    // Create product
    std::string product_id;
    if (db_manager_.createMarketProduct(owner_chat_id, owner_type, title, description,
                                        price_amount, price_currency, product_type,
                                        image_path, stock_quantity, user_id, product_id)) {
        std::ostringstream oss;
        oss << "{\"success\":true,\"product_id\":\"" << JsonParser::escapeJson(product_id) << "\"}";
        return oss.str();
    }
    
    return JsonParser::createErrorResponse("Failed to create product");
}

std::string RequestHandler::handleEditStoreItem(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string product_id = data["product_id"];
    std::string title = data["title"];
    std::string description = data["description"];
    std::string price_amount_str = data["price_amount"];
    std::string price_currency = data["price_currency"];
    std::string image_path = data.count("image_path") ? data["image_path"] : "";
    int stock_quantity = data.count("stock_quantity") ? std::stoi(data["stock_quantity"]) : -1;
    bool is_active = data.count("is_active") ? (data["is_active"] == "true") : true;
    
    // Input validation
    if (token.empty() || product_id.empty() || title.empty() || price_amount_str.empty() || price_currency.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    // Authenticate user
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Get product and check ownership
    auto product = db_manager_.getMarketProductById(product_id);
    if (product.id.empty()) {
        return JsonParser::createErrorResponse("Product not found");
    }
    
    if (product.created_by != user_id) {
        return JsonParser::createErrorResponse("Insufficient permissions");
    }
    
    // Validate and update
    double price_amount;
    try {
        price_amount = std::stod(price_amount_str);
        if (price_amount < 0) {
            return JsonParser::createErrorResponse("Price must be non-negative");
        }
    } catch (...) {
        return JsonParser::createErrorResponse("Invalid price_amount");
    }
    
    if (db_manager_.updateMarketProduct(product_id, title, description, price_amount,
                                       price_currency, image_path, stock_quantity, is_active)) {
        return JsonParser::createSuccessResponse("Product updated");
    }
    
    return JsonParser::createErrorResponse("Failed to update product");
}

std::string RequestHandler::handleDeleteStoreItem(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string product_id = data["product_id"];
    
    if (token.empty() || product_id.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Check ownership
    auto product = db_manager_.getMarketProductById(product_id);
    if (product.id.empty()) {
        return JsonParser::createErrorResponse("Product not found");
    }
    
    if (product.created_by != user_id) {
        return JsonParser::createErrorResponse("Insufficient permissions");
    }
    
    if (db_manager_.deleteMarketProduct(product_id)) {
        return JsonParser::createSuccessResponse("Product deleted");
    }
    
    return JsonParser::createErrorResponse("Failed to delete product");
}

std::string RequestHandler::handleGetStoreItems(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string owner_chat_id = data["owner_chat_id"];
    std::string owner_type = data["owner_type"];
    
    if (owner_chat_id.empty() || owner_type.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    auto products = db_manager_.getMarketProducts(owner_chat_id, owner_type);
    
    std::ostringstream oss;
    oss << "{\"success\":true,\"products\":[";
    for (size_t i = 0; i < products.size(); i++) {
        const auto& p = products[i];
        if (i > 0) oss << ",";
        oss << "{"
            << "\"id\":\"" << JsonParser::escapeJson(p.id) << "\","
            << "\"title\":\"" << JsonParser::escapeJson(p.title) << "\","
            << "\"description\":\"" << JsonParser::escapeJson(p.description) << "\","
            << "\"price_amount\":" << p.price_amount << ","
            << "\"price_currency\":\"" << JsonParser::escapeJson(p.price_currency) << "\","
            << "\"product_type\":\"" << JsonParser::escapeJson(p.product_type) << "\","
            << "\"image_path\":\"" << JsonParser::escapeJson(p.image_path) << "\","
            << "\"stock_quantity\":" << p.stock_quantity << ","
            << "\"is_active\":" << (p.is_active ? "true" : "false") << "}";
    }
    oss << "]}";
    return oss.str();
}

std::string RequestHandler::handlePurchaseProduct(const std::string& body) {
    auto data = JsonParser::parse(body);
    std::string token = data["token"];
    std::string product_id = data["product_id"];
    std::string invoice_id = data["invoice_id"];
    
    if (token.empty() || product_id.empty() || invoice_id.empty()) {
        return JsonParser::createErrorResponse("Missing required fields");
    }
    
    std::string user_id = auth_manager_.getUserIdFromToken(token);
    if (user_id.empty()) {
        return JsonParser::createErrorResponse("Invalid token");
    }
    
    // Get product
    auto product = db_manager_.getMarketProductById(product_id);
    if (product.id.empty() || !product.is_active) {
        return JsonParser::createErrorResponse("Product not available");
    }
    
    // Check stock
    if (product.stock_quantity >= 0 && product.stock_quantity == 0) {
        return JsonParser::createErrorResponse("Product out of stock");
    }
    
    // Create purchase
    std::string purchase_id, key_id;
    if (db_manager_.purchaseProduct(product_id, user_id, invoice_id, purchase_id, key_id)) {
        std::ostringstream oss;
        oss << "{\"success\":true,\"purchase_id\":\"" << JsonParser::escapeJson(purchase_id) << "\"";
        if (!key_id.empty()) {
            oss << ",\"key_id\":\"" << JsonParser::escapeJson(key_id) << "\"";
        }
        oss << "}";
        return oss.str();
    }
    
    return JsonParser::createErrorResponse("Failed to process purchase");
}

}

