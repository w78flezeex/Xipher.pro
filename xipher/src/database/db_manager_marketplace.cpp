// Marketplace database operations
// OWASP 2025 Security Compliant - All queries use prepared statements

#include "../include/database/db_manager.hpp"
#include "../include/utils/logger.hpp"
#include <sstream>
#include <cstring>
#include <libpq-fe.h>

namespace xipher {

bool DatabaseManager::createMarketProduct(const std::string& owner_chat_id, const std::string& owner_type,
                                         const std::string& title, const std::string& description,
                                         double price_amount, const std::string& price_currency,
                                         const std::string& product_type, const std::string& image_path,
                                         int stock_quantity, const std::string& created_by, std::string& product_id) {
    // Validate input to prevent injection
    if (owner_type != "group" && owner_type != "channel") {
        Logger::getInstance().error("Invalid owner_type for market product");
        return false;
    }
    if (price_currency != "USDT" && price_currency != "TON" && price_currency != "BTC" && price_currency != "RUB") {
        Logger::getInstance().error("Invalid price_currency");
        return false;
    }
    if (product_type != "digital_key" && product_type != "file" && product_type != "service" && product_type != "role") {
        Logger::getInstance().error("Invalid product_type");
        return false;
    }
    
    std::string price_str = std::to_string(price_amount);
    std::string stock_str = stock_quantity < 0 ? "NULL" : std::to_string(stock_quantity);
    std::string image_path_val = image_path.empty() ? "NULL" : image_path;
    
    const char* param_values[10] = {
        owner_chat_id.c_str(),
        owner_type.c_str(),
        title.c_str(),
        description.c_str(),
        price_str.c_str(),
        price_currency.c_str(),
        product_type.c_str(),
        image_path_val == "NULL" ? nullptr : image_path_val.c_str(),
        stock_str == "NULL" ? nullptr : stock_str.c_str(),
        created_by.c_str()
    };
    
    PGresult* res = db_->executePrepared("create_market_product", 10, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        Logger::getInstance().error("Failed to create market product: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    if (PQntuples(res) > 0) {
        product_id = std::string(PQgetvalue(res, 0, 0));
    }
    
    PQclear(res);
    return !product_id.empty();
}

bool DatabaseManager::updateMarketProduct(const std::string& product_id, const std::string& title,
                                         const std::string& description, double price_amount,
                                         const std::string& price_currency, const std::string& image_path,
                                         int stock_quantity, bool is_active) {
    std::string price_str = std::to_string(price_amount);
    std::string stock_str = stock_quantity < 0 ? "NULL" : std::to_string(stock_quantity);
    std::string active_str = is_active ? "TRUE" : "FALSE";
    std::string image_path_val = image_path.empty() ? "NULL" : image_path;
    
    const char* param_values[8] = {
        title.c_str(),
        description.c_str(),
        price_str.c_str(),
        price_currency.c_str(),
        image_path_val == "NULL" ? nullptr : image_path_val.c_str(),
        stock_str == "NULL" ? nullptr : stock_str.c_str(),
        active_str.c_str(),
        product_id.c_str()
    };
    
    PGresult* res = db_->executePrepared("update_market_product", 8, param_values);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("Failed to update market product: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool DatabaseManager::deleteMarketProduct(const std::string& product_id) {
    const char* param_values[1] = {product_id.c_str()};
    
    PGresult* res = db_->executePrepared("delete_market_product", 1, param_values);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("Failed to delete market product: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

std::vector<DatabaseManager::MarketProduct> DatabaseManager::getMarketProducts(
    const std::string& owner_chat_id, const std::string& owner_type) {
    std::vector<MarketProduct> products;
    
    const char* param_values[2] = {owner_chat_id.c_str(), owner_type.c_str()};
    
    PGresult* res = db_->executePrepared("get_market_products", 2, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        Logger::getInstance().error("Failed to get market products: " + db_->getLastError());
        if (res) PQclear(res);
        return products;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        MarketProduct product;
        product.id = std::string(PQgetvalue(res, i, 0));
        product.owner_chat_id = std::string(PQgetvalue(res, i, 1));
        product.owner_type = std::string(PQgetvalue(res, i, 2));
        product.title = std::string(PQgetvalue(res, i, 3));
        product.description = PQgetvalue(res, i, 4) ? std::string(PQgetvalue(res, i, 4)) : "";
        product.price_amount = std::stod(PQgetvalue(res, i, 5));
        product.price_currency = std::string(PQgetvalue(res, i, 6));
        product.product_type = std::string(PQgetvalue(res, i, 7));
        product.image_path = PQgetvalue(res, i, 8) ? std::string(PQgetvalue(res, i, 8)) : "";
        product.is_active = std::string(PQgetvalue(res, i, 9)) == "t";
        product.stock_quantity = PQgetvalue(res, i, 10) ? std::stoi(PQgetvalue(res, i, 10)) : -1;
        product.created_at = std::string(PQgetvalue(res, i, 11));
        product.created_by = std::string(PQgetvalue(res, i, 12));
        products.push_back(product);
    }
    
    PQclear(res);
    return products;
}

DatabaseManager::MarketProduct DatabaseManager::getMarketProductById(const std::string& product_id) {
    MarketProduct product;
    
    const char* param_values[1] = {product_id.c_str()};
    
    PGresult* res = db_->executePrepared("get_market_product_by_id", 1, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
        Logger::getInstance().error("Failed to get market product: " + db_->getLastError());
        if (res) PQclear(res);
        return product;
    }
    
    product.id = std::string(PQgetvalue(res, 0, 0));
    product.owner_chat_id = std::string(PQgetvalue(res, 0, 1));
    product.owner_type = std::string(PQgetvalue(res, 0, 2));
    product.title = std::string(PQgetvalue(res, 0, 3));
    product.description = PQgetvalue(res, 0, 4) ? std::string(PQgetvalue(res, 0, 4)) : "";
    product.price_amount = std::stod(PQgetvalue(res, 0, 5));
    product.price_currency = std::string(PQgetvalue(res, 0, 6));
    product.product_type = std::string(PQgetvalue(res, 0, 7));
    product.image_path = PQgetvalue(res, 0, 8) ? std::string(PQgetvalue(res, 0, 8)) : "";
    product.is_active = std::string(PQgetvalue(res, 0, 9)) == "t";
    product.stock_quantity = PQgetvalue(res, 0, 10) ? std::stoi(PQgetvalue(res, 0, 10)) : -1;
    product.created_at = std::string(PQgetvalue(res, 0, 11));
    product.created_by = std::string(PQgetvalue(res, 0, 12));
    
    PQclear(res);
    return product;
}

bool DatabaseManager::addProductKey(const std::string& product_id, const std::string& key_value) {
    const char* param_values[2] = {product_id.c_str(), key_value.c_str()};
    
    PGresult* res = db_->executePrepared("add_product_key", 2, param_values);
    if (!res || PQresultStatus(res) != PGRES_COMMAND_OK) {
        Logger::getInstance().error("Failed to add product key: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}

bool DatabaseManager::purchaseProduct(const std::string& product_id, const std::string& buyer_id,
                                     const std::string& invoice_id, std::string& purchase_id, std::string& key_id) {
    // Get product info first
    MarketProduct product = getMarketProductById(product_id);
    if (product.id.empty()) {
        return false;
    }
    
    // Calculate platform fee (5% default)
    double platform_fee = product.price_amount * 0.05;
    
    // Get seller_id from owner_chat_id (need to check if it's a group/channel and get creator)
    // For simplicity, assuming owner_chat_id is the seller for now
    std::string seller_id = product.created_by;
    
    std::string price_str = std::to_string(product.price_amount);
    std::string fee_str = std::to_string(platform_fee);
    
    const char* param_values[6] = {
        product_id.c_str(),
        buyer_id.c_str(),
        seller_id.c_str(),
        price_str.c_str(),
        product.price_currency.c_str(),
        invoice_id.c_str()
    };
    
    PGresult* res = db_->executePrepared("create_product_purchase", 6, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        Logger::getInstance().error("Failed to create purchase: " + db_->getLastError());
        if (res) PQclear(res);
        return false;
    }
    
    if (PQntuples(res) > 0) {
        purchase_id = std::string(PQgetvalue(res, 0, 0));
    }
    
    PQclear(res);
    
    // If product is digital_key, get and assign a key
    if (product.product_type == "digital_key" && !purchase_id.empty()) {
        // Get unused key
        const char* key_params[1] = {product_id.c_str()};
        PGresult* key_res = db_->executePrepared("get_unused_product_key", 1, key_params);
        if (key_res && PQresultStatus(key_res) == PGRES_TUPLES_OK && PQntuples(key_res) > 0) {
            key_id = std::string(PQgetvalue(key_res, 0, 0));
            std::string key_value = std::string(PQgetvalue(key_res, 0, 1));
            
            // Mark key as used
            const char* use_params[2] = {key_id.c_str(), buyer_id.c_str()};
            db_->executePrepared("mark_product_key_used", 2, use_params);
            
            // Update purchase with key_id
            const char* update_params[2] = {purchase_id.c_str(), key_id.c_str()};
            db_->executePrepared("update_purchase_key", 2, update_params);
        }
        if (key_res) PQclear(key_res);
    }
    
    return !purchase_id.empty();
}

std::vector<DatabaseManager::ProductPurchase> DatabaseManager::getUserPurchases(const std::string& user_id) {
    std::vector<ProductPurchase> purchases;
    
    const char* param_values[1] = {user_id.c_str()};
    
    PGresult* res = db_->executePrepared("get_user_purchases", 1, param_values);
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK) {
        Logger::getInstance().error("Failed to get user purchases: " + db_->getLastError());
        if (res) PQclear(res);
        return purchases;
    }
    
    int rows = PQntuples(res);
    for (int i = 0; i < rows; i++) {
        ProductPurchase purchase;
        purchase.id = std::string(PQgetvalue(res, i, 0));
        purchase.product_id = std::string(PQgetvalue(res, i, 1));
        purchase.buyer_id = std::string(PQgetvalue(res, i, 2));
        purchase.seller_id = std::string(PQgetvalue(res, i, 3));
        purchase.price_amount = std::stod(PQgetvalue(res, i, 4));
        purchase.price_currency = std::string(PQgetvalue(res, i, 5));
        purchase.platform_fee = std::stod(PQgetvalue(res, i, 6));
        purchase.status = std::string(PQgetvalue(res, i, 7));
        purchase.invoice_id = PQgetvalue(res, i, 8) ? std::string(PQgetvalue(res, i, 8)) : "";
        purchase.key_id = PQgetvalue(res, i, 9) ? std::string(PQgetvalue(res, i, 9)) : "";
        purchase.created_at = std::string(PQgetvalue(res, i, 10));
        purchases.push_back(purchase);
    }
    
    PQclear(res);
    return purchases;
}

} // namespace xipher

