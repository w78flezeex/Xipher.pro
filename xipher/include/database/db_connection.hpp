#ifndef DB_CONNECTION_HPP
#define DB_CONNECTION_HPP

#include <string>
#include <memory>
#include <libpq-fe.h>

namespace xipher {

class DatabaseConnection {
public:
    DatabaseConnection(const std::string& host, 
                      const std::string& port,
                      const std::string& dbname,
                      const std::string& user,
                      const std::string& password);
    ~DatabaseConnection();
    
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    PGresult* executeQuery(const std::string& query);
    PGresult* executePrepared(const std::string& stmt_name,
                              int n_params,
                              const char* const* param_values);
    
    void prepareStatement(const std::string& stmt_name,
                         const std::string& query);
    
    std::string getLastError() const;
    
    // Get raw connection for direct queries
    PGconn* getConnection() const { return conn_; }
    
private:
    std::string host_;
    std::string port_;
    std::string dbname_;
    std::string user_;
    std::string password_;
    PGconn* conn_;
    
    void logError();
};

} // namespace xipher

#endif // DB_CONNECTION_HPP

