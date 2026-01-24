#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP

#include <string>
#include <memory>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <map>
#include <mutex>
#include <memory>
#include <unordered_map>
#include "../database/db_manager.hpp"
#include "../auth/auth_manager.hpp"
#include "../voip/voip_access_control.hpp"
#include "../bots/bot_scheduler.hpp"
#include "request_handler.hpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

namespace xipher {

class HttpServer {
public:
    HttpServer(const std::string& address, unsigned short port);
    ~HttpServer();
    
    bool start();
    void stop();
    
private:
    std::string address_;
    unsigned short port_;
    net::io_context ioc_;
    std::unique_ptr<tcp::acceptor> acceptor_;
    std::unique_ptr<DatabaseManager> db_manager_;
    std::unique_ptr<AuthManager> auth_manager_;
    std::unique_ptr<RequestHandler> request_handler_;
    std::unique_ptr<VoipAccessControl> voip_access_control_;
    BotScheduler bot_scheduler_;
    bool running_;
    
    // WebSocket connections storage (user_id -> websocket stream)
    std::map<std::string, std::weak_ptr<websocket::stream<beast::tcp_stream>>> ws_connections_;
    std::mutex ws_connections_mutex_;
    std::unordered_map<void*, std::string> ws_user_ids_;
    std::mutex ws_user_ids_mutex_;
    
    void acceptConnections();
    void handleConnection(std::shared_ptr<tcp::socket> socket);
    void readRequest(std::shared_ptr<tcp::socket> socket,
                    std::shared_ptr<beast::flat_buffer> buffer);
    void processRequest(std::shared_ptr<tcp::socket> socket,
                       http::request<http::string_body> req);
    void sendResponse(std::shared_ptr<tcp::socket> socket,
                     http::response<http::string_body> res);
    void handleWebSocketUpgrade(std::shared_ptr<tcp::socket> socket,
                               http::request<http::string_body> req);
    void doReadWebSocket(std::shared_ptr<websocket::stream<beast::tcp_stream>> ws);
    void handleWebSocketMessage(std::shared_ptr<websocket::stream<beast::tcp_stream>> ws,
                               const std::string& message);
    void registerWebSocketConnection(const std::string& user_id, 
                                     std::shared_ptr<websocket::stream<beast::tcp_stream>> ws);
    void unregisterWebSocketConnection(const std::string& user_id);
    void sendToUser(const std::string& user_id, const std::string& message);
    std::string getWebSocketUserId(std::shared_ptr<websocket::stream<beast::tcp_stream>> ws);
};

} // namespace xipher

#endif // HTTP_SERVER_HPP
