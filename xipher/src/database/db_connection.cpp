#include "../include/database/db_connection.hpp"
#include "../include/utils/logger.hpp"
#include <cstring>

namespace xipher {

DatabaseConnection::DatabaseConnection(const std::string& host,
                                     const std::string& port,
                                     const std::string& dbname,
                                     const std::string& user,
                                     const std::string& password)
    : host_(host), port_(port), dbname_(dbname), user_(user), password_(password), conn_(nullptr) {
}

DatabaseConnection::~DatabaseConnection() {
    disconnect();
}

bool DatabaseConnection::connect() {
    std::string conninfo = "host=" + host_ +
                          " port=" + port_ +
                          " dbname=" + dbname_ +
                          " user=" + user_ +
                          " password=" + password_;
    
    conn_ = PQconnectdb(conninfo.c_str());
    
    if (PQstatus(conn_) != CONNECTION_OK) {
        logError();
        return false;
    }
    
    Logger::getInstance().info("Connected to PostgreSQL database");
    return true;
}

void DatabaseConnection::disconnect() {
    if (conn_) {
        PQfinish(conn_);
        conn_ = nullptr;
    }
}

bool DatabaseConnection::isConnected() const {
    return conn_ != nullptr && PQstatus(conn_) == CONNECTION_OK;
}

PGresult* DatabaseConnection::executeQuery(const std::string& query) {
    if (!isConnected()) {
        Logger::getInstance().error("Database not connected");
        return nullptr;
    }
    
    PGresult* res = PQexec(conn_, query.c_str());
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK) {
        logError();
        PQclear(res);
        return nullptr;
    }
    
    return res;
}

PGresult* DatabaseConnection::executePrepared(const std::string& stmt_name,
                                             int n_params,
                                             const char* const* param_values) {
    if (!isConnected()) {
        Logger::getInstance().error("Database not connected");
        return nullptr;
    }
    
    // Создаем массивы для длин параметров (нужны для NULL значений)
    int* param_lengths = new int[n_params];
    int* param_formats = new int[n_params];
    
    for (int i = 0; i < n_params; i++) {
        if (param_values[i] == nullptr) {
            param_lengths[i] = -1;  // -1 означает NULL значение в PostgreSQL
        } else {
            param_lengths[i] = strlen(param_values[i]);
        }
        param_formats[i] = 0;  // 0 = text format, 1 = binary format
    }
    
    PGresult* res = PQexecPrepared(conn_, stmt_name.c_str(), n_params, param_values, param_lengths, param_formats, 0);
    
    delete[] param_lengths;
    delete[] param_formats;
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK) {
        std::string error_msg = PQresultErrorMessage(res) ? PQresultErrorMessage(res) : "Unknown error";
        Logger::getInstance().error("PostgreSQL prepared statement error (" + stmt_name + "): " + error_msg);
        PQclear(res);
        return nullptr;
    }
    
    return res;
}

void DatabaseConnection::prepareStatement(const std::string& stmt_name, const std::string& query) {
    // Log preparation attempt for debugging
    Logger::getInstance().info("Preparing statement: " + stmt_name);
    if (!isConnected()) {
        Logger::getInstance().error("Database not connected");
        return;
    }
    
    // Сначала пытаемся удалить существующий statement (если есть)
    std::string deallocate = "DEALLOCATE " + stmt_name;
    PGresult* dealloc_res = PQexec(conn_, deallocate.c_str());
    PQclear(dealloc_res); // Игнорируем ошибку, если statement не существует
    
    // Подсчитываем количество параметров ($1, $2, ...)
    int param_count = 0;
    size_t pos = 0;
    while ((pos = query.find('$', pos)) != std::string::npos) {
        pos++;
        if (pos < query.length() && std::isdigit(query[pos])) {
            int num = 0;
            while (pos < query.length() && std::isdigit(query[pos])) {
                num = num * 10 + (query[pos] - '0');
                pos++;
            }
            if (num > param_count) param_count = num;
        }
    }
    
    // Теперь создаем statement с правильным количеством параметров
    PGresult* res = PQprepare(conn_, stmt_name.c_str(), query.c_str(), param_count, nullptr);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::string error = PQerrorMessage(conn_);
        Logger::getInstance().error("Failed to prepare statement '" + stmt_name + "' with " + std::to_string(param_count) + " params: " + error);
        // Игнорируем ошибку "already exists", это нормально
        if (error.find("already exists") == std::string::npos && error.find("does not exist") == std::string::npos) {
            logError();
        }
    } else {
        Logger::getInstance().info("Successfully prepared statement '" + stmt_name + "' with " + std::to_string(param_count) + " parameters");
    }
    
    PQclear(res);
}

std::string DatabaseConnection::getLastError() const {
    if (conn_) {
        return PQerrorMessage(conn_);
    }
    return "No connection";
}

void DatabaseConnection::logError() {
    if (conn_) {
        Logger::getInstance().error("PostgreSQL error: " + std::string(PQerrorMessage(conn_)));
    }
}

} // namespace xipher

