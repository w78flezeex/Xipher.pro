#include "../include/server/http_server.hpp"
#include "../include/utils/logger.hpp"
#include "../include/utils/json_parser.hpp"
#include "../include/voip/voip_access_control.hpp"
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <sstream>
#include <algorithm>

namespace {
constexpr const char* kSessionTokenCookieName = "xipher_token";
const std::string kSessionTokenPlaceholder = "cookie";
// Allow ~10 GB uploads after base64 overhead.
constexpr auto kMaxRequestBodySize = 16ULL * 1024 * 1024 * 1024; // 16 GB

std::string extractCookieValue(const std::string& cookie_header, const std::string& name) {
    const std::string needle = name + "=";
    size_t pos = cookie_header.find(needle);
    if (pos == std::string::npos) {
        return "";
    }
    size_t start = pos + needle.size();
    size_t end = cookie_header.find(';', start);
    if (end == std::string::npos) {
        end = cookie_header.size();
    }
    return cookie_header.substr(start, end - start);
}
} // namespace

namespace xipher {

HttpServer::HttpServer(const std::string& address, unsigned short port)
    : address_(address), port_(port), running_(false) {
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    try {
        // Initialize database (use env overrides, fallback to sane defaults)
        const char* env_host = std::getenv("XIPHER_DB_HOST");
        const char* env_port = std::getenv("XIPHER_DB_PORT");
        const char* env_name = std::getenv("XIPHER_DB_NAME");
        const char* env_user = std::getenv("XIPHER_DB_USER");
        const char* env_pass = std::getenv("XIPHER_DB_PASSWORD");

        std::string db_host = env_host ? env_host : "localhost";
        std::string db_port = env_port ? env_port : "5432";
        std::string db_name = env_name ? env_name : "xipher";
        std::string db_user = env_user ? env_user : "xipher";
        std::string db_pass = env_pass ? env_pass : "xipher";

        db_manager_ = std::make_unique<DatabaseManager>(db_host, db_port, db_name, db_user, db_pass);
        if (!db_manager_->initialize()) {
            Logger::getInstance().error("Failed to initialize database");
            return false;
        }

        // Background bot scheduler (reminders, etc.)
        bot_scheduler_.start();
        
        // Initialize auth manager
        auth_manager_ = std::make_unique<AuthManager>(*db_manager_);
        
        // Initialize VoIP access control (Loyalty Beta cutoff date)
        // TODO: Load cutoff timestamp from config file or environment variable
        // Example: 1704067200 = 2024-01-01 00:00:00 UTC
        // Users registered AFTER this date are blocked from VoIP
        // 
        // To DISABLE access control (allow all users), set enabled=false:
        // voip_access_control_ = std::make_unique<VoipAccessControl>(0, false);
        //
        // To ENABLE access control with cutoff date:
        int64_t voip_cutoff_timestamp = 1704067200; // Default: 2024-01-01
        bool voip_access_enabled = false; // Set to true to enable access control, false to allow all users
        voip_access_control_ = std::make_unique<VoipAccessControl>(voip_cutoff_timestamp, voip_access_enabled);
        
        // Initialize request handler
        request_handler_ = std::make_unique<RequestHandler>(*db_manager_, *auth_manager_);
        request_handler_->setWebSocketSender([this](const std::string& user_id, const std::string& message) {
            this->sendToUser(user_id, message);
        });
        
        // Create acceptor
        tcp::endpoint endpoint(net::ip::make_address(address_), port_);
        acceptor_ = std::make_unique<tcp::acceptor>(ioc_, endpoint);
        
        running_ = true;
        Logger::getInstance().info("HTTP Server started on " + address_ + ":" + std::to_string(port_));
        
        acceptConnections();
        
        // Run IO context
        ioc_.run();
        
        return true;
    } catch (const std::exception& e) {
        Logger::getInstance().error("Server error: " + std::string(e.what()));
        return false;
    }
}

void HttpServer::stop() {
    if (running_) {
        running_ = false;
        bot_scheduler_.stop();
        ioc_.stop();
        Logger::getInstance().info("HTTP Server stopped");
    }
}

void HttpServer::acceptConnections() {
    if (!running_) return;
    
    auto socket = std::make_shared<tcp::socket>(ioc_);
    
    acceptor_->async_accept(*socket,
        [this, socket](beast::error_code ec) {
            if (!ec) {
                handleConnection(socket);
            } else {
                Logger::getInstance().error("Accept error: " + ec.message());
            }
            acceptConnections();
        });
}

void HttpServer::handleConnection(std::shared_ptr<tcp::socket> socket) {
    auto buffer = std::make_shared<beast::flat_buffer>();
    auto parser = std::make_shared<http::request_parser<http::string_body>>();
    parser->body_limit(kMaxRequestBodySize);

    http::async_read(*socket, *buffer, *parser,
        [this, socket, buffer, parser](beast::error_code ec, std::size_t) {
            if (!ec) {
                try {
                    processRequest(socket, parser->release());
                } catch (const std::exception& e) {
                    Logger::getInstance().error("Unhandled exception in handleConnection: " + std::string(e.what()));
                } catch (...) {
                    Logger::getInstance().error("Unhandled unknown exception in handleConnection");
                }
                return;
            }
            if (ec == http::error::body_limit) {
                Logger::getInstance().warning("Request body too large");
                http::response<http::string_body> res;
                res.result(http::status::payload_too_large);
                res.set(http::field::content_type, "application/json");
                res.set(http::field::access_control_allow_origin, "*");
                res.body() = JsonParser::createErrorResponse("Request body too large");
                res.prepare_payload();
                sendResponse(socket, res);
                return;
            }
            Logger::getInstance().error("Read error: " + ec.message());
        });
}

void HttpServer::readRequest(std::shared_ptr<tcp::socket> socket,
                            std::shared_ptr<beast::flat_buffer> buffer) {
    auto parser = std::make_shared<http::request_parser<http::string_body>>();
    parser->body_limit(kMaxRequestBodySize);

    http::async_read(*socket, *buffer, *parser,
        [this, socket, buffer, parser](beast::error_code ec, std::size_t) {
            if (!ec) {
                try {
                    processRequest(socket, parser->release());
                } catch (const std::exception& e) {
                    Logger::getInstance().error("Unhandled exception in readRequest: " + std::string(e.what()));
                } catch (...) {
                    Logger::getInstance().error("Unhandled unknown exception in readRequest");
                }
                return;
            }
            if (ec == http::error::body_limit) {
                Logger::getInstance().warning("Request body too large");
                http::response<http::string_body> res;
                res.result(http::status::payload_too_large);
                res.set(http::field::content_type, "application/json");
                res.set(http::field::access_control_allow_origin, "*");
                res.body() = JsonParser::createErrorResponse("Request body too large");
                res.prepare_payload();
                sendResponse(socket, res);
                return;
            }
            Logger::getInstance().error("Read error: " + ec.message());
        });
}

void HttpServer::processRequest(std::shared_ptr<tcp::socket> socket,
                               http::request<http::string_body> req) {
    auto isSecureRequest = [&req]() -> bool {
        auto it = req.find("X-Forwarded-Proto");
        if (it != req.end()) {
            std::string value = std::string(it->value());
            std::transform(value.begin(), value.end(), value.begin(), ::tolower);
            if (value.find("https") != std::string::npos) return true;
        }
        it = req.find("X-Forwarded-Scheme");
        if (it != req.end()) {
            std::string value = std::string(it->value());
            std::transform(value.begin(), value.end(), value.begin(), ::tolower);
            if (value.find("https") != std::string::npos) return true;
        }
        it = req.find("X-Forwarded-SSL");
        if (it != req.end()) {
            std::string value = std::string(it->value());
            std::transform(value.begin(), value.end(), value.begin(), ::tolower);
            if (value == "on" || value == "1" || value == "true") return true;
        }
        it = req.find("Forwarded");
        if (it != req.end()) {
            std::string value = std::string(it->value());
            std::transform(value.begin(), value.end(), value.begin(), ::tolower);
            if (value.find("proto=https") != std::string::npos) return true;
        }
        return false;
    };

    auto applySecurityHeaders = [&](http::response<http::string_body>& res) {
        const std::string csp =
            "default-src 'self'; "
            "base-uri 'self'; "
            "object-src 'none'; "
            "frame-ancestors 'none'; "
            "script-src 'self' 'unsafe-inline' https://cdnjs.cloudflare.com https://unpkg.com https://cdn.jsdelivr.net https://www.gstatic.com; "
            "style-src 'self' 'unsafe-inline' https://fonts.googleapis.com https://cdnjs.cloudflare.com https://cdn.jsdelivr.net; "
            "font-src 'self' https://fonts.gstatic.com https://r2cdn.perplexity.ai data:; "
            "img-src 'self' data: blob:; "
            "connect-src 'self' wss: ws: https://cdn.jsdelivr.net https://unpkg.com https://fcmregistrations.googleapis.com https://firebaseinstallations.googleapis.com; "
            "form-action 'self' https://yoomoney.ru https://checkout.stripe.com; "
            "media-src 'self' blob:; ";

        res.set("Content-Security-Policy", csp);
        res.set("X-Content-Type-Options", "nosniff");
        res.set("X-Frame-Options", "DENY");
        res.set("X-XSS-Protection", "1; mode=block");
        res.set("Referrer-Policy", "strict-origin-when-cross-origin");
        res.set("Permissions-Policy", "geolocation=(), microphone=(self), camera=(self), display-capture=(self)");
        if (isSecureRequest()) {
            res.set("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
        }
    };

    try {
    // Check for WebSocket upgrade request
    std::string path = std::string(req.target());
    if (path == "/ws") {
        // Проверяем заголовки для WebSocket upgrade
        bool is_websocket = false;
        auto upgrade_it = req.find(http::field::upgrade);
        auto connection_it = req.find(http::field::connection);
        
        if (upgrade_it != req.end() && boost::beast::iequals(upgrade_it->value(), "websocket")) {
            if (connection_it != req.end()) {
                std::string connection = std::string(connection_it->value());
                std::transform(connection.begin(), connection.end(), connection.begin(), ::tolower);
                if (connection.find("upgrade") != std::string::npos) {
                    is_websocket = true;
                }
            }
        }
        
        if (is_websocket) {
            // Handle WebSocket upgrade
            handleWebSocketUpgrade(socket, req);
            return;
        }
    }
    
    std::string client_ip;
    try {
        client_ip = socket->remote_endpoint().address().to_string();
    } catch (...) {
        client_ip = "";
    }

    // Extract headers
    std::map<std::string, std::string> headers;
    for (const auto& header : req) {
        headers[std::string(header.name_string())] = std::string(header.value());
    }
    if (!client_ip.empty()) {
        headers["X-Client-IP"] = client_ip;
    }
    
    // Convert method to string
    std::string method = std::string(to_string(req.method()));
    std::string body = req.body();
    
    // Handle request
    std::string response_str = request_handler_->handleRequest(method, path, headers, body);
    
    // Parse response
    http::response<http::string_body> res;
    
    // Check if response is already a full HTTP response (for static files)
    if (response_str.find("HTTP/1.1") == 0) {
        // Parse the HTTP response string
        std::istringstream response_stream(response_str);
        std::string line;
        
        // Read status line (e.g. "HTTP/1.1 404 Not Found")
        std::getline(response_stream, line);
        int status_code = 200;
        try {
            std::istringstream status_stream(line);
            std::string http_version;
            status_stream >> http_version >> status_code;
            if (!status_stream || status_code < 100 || status_code > 599) {
                status_code = 200;
            }
        } catch (...) {
            status_code = 200;
        }
        
        // Read headers
        std::map<std::string, std::string> response_headers;
        while (std::getline(response_stream, line) && line != "\r" && !line.empty()) {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                // Trim whitespace
                value.erase(0, value.find_first_not_of(" \t\r\n"));
                value.erase(value.find_last_not_of(" \t\r\n") + 1);
                response_headers[key] = value;
            }
        }
        
        // Read body
        std::ostringstream body_stream;
        body_stream << response_stream.rdbuf();
        std::string response_body = body_stream.str();
        
        // Build Beast response (preserve status code from status line)
        res.result(static_cast<http::status>(status_code));
        for (const auto& header : response_headers) {
            if (header.first == "Content-Type") {
                res.set(http::field::content_type, header.second);
            } else if (header.first == "Content-Length") {
                // Will be set by prepare_payload
            } else if (header.first == "Location") {
                res.set(http::field::location, header.second);
            } else if (header.first == "Cache-Control") {
                res.set(http::field::cache_control, header.second);
            } else if (header.first == "Access-Control-Allow-Origin") {
                res.set(http::field::access_control_allow_origin, header.second);
            } else {
                // Set custom headers
                res.set(header.first, header.second);
            }
        }
        
        applySecurityHeaders(res);
        
        res.body() = response_body;
        res.prepare_payload();
        
        sendResponse(socket, res);
        return;
    }
    
    // It's a JSON response, wrap it in HTTP
    res.result(http::status::ok);
    res.set(http::field::content_type, "application/json");
    
    // Security headers (OWASP 2025 best practices)
    res.set(http::field::access_control_allow_origin, "*");
    applySecurityHeaders(res);
    
    res.body() = response_str;
    res.prepare_payload();
    
    sendResponse(socket, res);
    } catch (const std::exception& e) {
        Logger::getInstance().error("Unhandled exception in processRequest: " + std::string(e.what()));
        http::response<http::string_body> res;
        res.result(http::status::internal_server_error);
        res.set(http::field::content_type, "application/json");
        res.set(http::field::access_control_allow_origin, "*");
        res.body() = JsonParser::createErrorResponse("Internal server error");
        res.prepare_payload();
        applySecurityHeaders(res);
        sendResponse(socket, res);
    } catch (...) {
        Logger::getInstance().error("Unhandled unknown exception in processRequest");
        http::response<http::string_body> res;
        res.result(http::status::internal_server_error);
        res.set(http::field::content_type, "application/json");
        res.set(http::field::access_control_allow_origin, "*");
        res.body() = JsonParser::createErrorResponse("Internal server error");
        res.prepare_payload();
        applySecurityHeaders(res);
        sendResponse(socket, res);
    }
}

void HttpServer::sendResponse(std::shared_ptr<tcp::socket> socket,
                              http::response<http::string_body> res) {
    auto sp = std::make_shared<http::response<http::string_body>>(std::move(res));
    
    http::async_write(*socket, *sp,
        [this, socket, sp](beast::error_code ec, std::size_t) {
            if (ec) {
                Logger::getInstance().error("Write error: " + ec.message());
            }
            socket->shutdown(tcp::socket::shutdown_send, ec);
        });
}

void HttpServer::handleWebSocketUpgrade(std::shared_ptr<tcp::socket> socket,
                                        http::request<http::string_body> req) {
    // Создаем WebSocket stream из сокета
    auto ws = std::make_shared<websocket::stream<beast::tcp_stream>>(beast::tcp_stream(std::move(*socket)));
    
    // Устанавливаем таймауты
    ws->set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    
    // Принимаем WebSocket handshake
    ws->async_accept(req,
        [this, ws, req](beast::error_code ec) {
            if (ec) {
                Logger::getInstance().error("WebSocket accept error: " + ec.message());
                return;
            }
            
            Logger::getInstance().info("WebSocket connection accepted");

            std::string cookie_header;
            auto cookie_it = req.find(http::field::cookie);
            if (cookie_it != req.end()) {
                cookie_header = std::string(cookie_it->value());
            }
            std::string token = extractCookieValue(cookie_header, kSessionTokenCookieName);
            if (!token.empty() && token != kSessionTokenPlaceholder) {
                std::string user_id = auth_manager_->getUserIdFromToken(token);
                if (!user_id.empty()) {
                    registerWebSocketConnection(user_id, ws);
                }
            }
            
            // Начинаем чтение сообщений
            doReadWebSocket(ws);
        });
}

void HttpServer::doReadWebSocket(std::shared_ptr<websocket::stream<beast::tcp_stream>> ws) {
    auto buffer = std::make_shared<beast::flat_buffer>();
    
    ws->async_read(*buffer,
        [this, ws, buffer](beast::error_code ec, std::size_t) {
            if (ec == websocket::error::closed) {
                Logger::getInstance().info("WebSocket connection closed");
                const std::string user_id = getWebSocketUserId(ws);
                if (!user_id.empty()) {
                    unregisterWebSocketConnection(user_id);
                } else {
                    std::lock_guard<std::mutex> lock(ws_connections_mutex_);
                    for (auto it = ws_connections_.begin(); it != ws_connections_.end();) {
                        if (it->second.lock() == ws) {
                            ws_connections_.erase(it++);
                        } else {
                            ++it;
                        }
                    }
                    std::lock_guard<std::mutex> lock_users(ws_user_ids_mutex_);
                    ws_user_ids_.erase(ws.get());
                }
                return;
            }
            if (ec) {
                Logger::getInstance().error("WebSocket read error: " + ec.message());
                return;
            }
            
            // Парсим сообщение
            std::string message(static_cast<const char*>(buffer->data().data()), buffer->data().size());
            buffer->consume(buffer->size());
            
            Logger::getInstance().info("WebSocket message received: " + message);
            
            // Обрабатываем сообщение
            handleWebSocketMessage(ws, message);
            
            // Продолжаем чтение
            doReadWebSocket(ws);
        });
}

void HttpServer::handleWebSocketMessage(std::shared_ptr<websocket::stream<beast::tcp_stream>> ws,
                                        const std::string& message) {
    try {
        Logger::getInstance().info("========== WebSocket message received ==========");
        Logger::getInstance().info("Raw message: " + message);
        
        // Парсим JSON
        auto data = JsonParser::parse(message);
        std::string type = data["type"];
        
        Logger::getInstance().info("Message type: " + type);
        
        if (type == "auth") {
            std::string token = data["token"];
            if (token == kSessionTokenPlaceholder) {
                token.clear();
            }
            std::string user_id = token.empty() ? "" : auth_manager_->getUserIdFromToken(token);
            if (user_id.empty()) {
                user_id = getWebSocketUserId(ws);
            }
            
            if (!user_id.empty()) {
                std::string push_token = data.count("push_token") ? data["push_token"] : "";
                std::string platform = data.count("platform") ? data["platform"] : "android";
                if (!push_token.empty() && db_manager_) {
                    if (db_manager_->upsertPushToken(user_id, push_token, platform)) {
                        Logger::getInstance().info("Push token registered via WS for user " + user_id + " (" + platform + ")");
                    } else {
                        Logger::getInstance().warning("Push token WS registration failed for user " + user_id);
                    }
                }

                // Регистрируем соединение
                registerWebSocketConnection(user_id, ws);
                
                std::string response = "{\"type\":\"auth_success\",\"user_id\":\"" + user_id + "\"}";
                ws->async_write(net::buffer(response),
                    [](beast::error_code ec, std::size_t) {
                        if (ec) {
                            Logger::getInstance().error("WebSocket write error: " + ec.message());
                        }
                    });
            } else {
                std::string response = "{\"type\":\"auth_error\",\"error\":\"Invalid token\"}";
                ws->async_write(net::buffer(response),
                    [](beast::error_code, std::size_t) {});
            }
        } else if (type == "call_init" || type == "call_offer" || type == "call_answer" || type == "call_ice_candidate" || type == "call_end") {
            // Обрабатываем звонки через WebSocket
            std::string token = data["token"];
            if (token == kSessionTokenPlaceholder) {
                token.clear();
            }
            std::string user_id = token.empty() ? "" : auth_manager_->getUserIdFromToken(token);
            if (user_id.empty()) {
                user_id = getWebSocketUserId(ws);
            }
            
            Logger::getInstance().info("Processing " + type + " from user " + user_id);
            
            if (user_id.empty()) {
                std::string error_response = "{\"type\":\"call_error\",\"error_code\":1002,\"error_message\":\"Not authenticated\"}";
                ws->async_write(net::buffer(error_response),
                    [](beast::error_code, std::size_t) {});
                return;
            }
            
            // VoIP Access Control Check - CRITICAL BUSINESS CONSTRAINT
            // Check if user has access to VoIP features (Loyalty Beta)
            if (voip_access_control_ && !voip_access_control_->checkUserAccess(user_id, *db_manager_)) {
                std::string error_response = "{\"type\":\"call_error\",\"error_code\":1001,\"error_message\":\"VoIP feature is currently in Loyalty Beta. This feature is not available for your account.\"}";
                ws->async_write(net::buffer(error_response),
                    [](beast::error_code, std::size_t) {});
                Logger::getInstance().info("VoIP access DENIED for user: " + user_id);
                return;
            }
            
            // Получаем target_user_id из сообщения
            std::string target_user_id;
            if (data.count("target_user_id")) {
                target_user_id = data["target_user_id"];
                Logger::getInstance().info("Found target_user_id in message: " + target_user_id);
            } else if (data.count("receiver_id")) {
                target_user_id = data["receiver_id"];
                Logger::getInstance().info("Found receiver_id in message: " + target_user_id);
            } else {
                Logger::getInstance().warning("No target_user_id or receiver_id in " + type + " message");
                Logger::getInstance().warning("Message keys: " + [&data]() {
                    std::string keys;
                    for (const auto& pair : data) {
                        keys += pair.first + " ";
                    }
                    return keys;
                }());
            }
            
            Logger::getInstance().info("Target user ID: " + target_user_id);
            Logger::getInstance().info("From user ID: " + user_id);
            
            if (!target_user_id.empty()) {
                // Получаем имя пользователя из базы данных
                std::string from_username = "Пользователь";
                try {
                    auto user = db_manager_->getUserById(user_id);
                    if (!user.id.empty()) {
                        from_username = user.username;
                    }
                } catch (...) {
                    Logger::getInstance().warning("Could not get username for user: " + user_id);
                }
                
                // Пересылаем сообщение получателю через WebSocket, нормализуя JSON чтобы не ломать парсинг на клиенте
                std::string call_type = data.count("call_type") ? data["call_type"] : "video";

                auto decodePayload = [&](const std::string& value, const std::string& encoding_key) -> std::string {
                    std::string enc = "";
                    auto it = data.find(encoding_key);
                    if (it != data.end()) enc = it->second;
                    bool looks_b64 = !value.empty() &&
                        (value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=") == std::string::npos) &&
                        (value.size() % 4 == 0);
                    if (enc == "b64" || looks_b64) {
                        std::string decoded = request_handler_->base64Decode(value);
                        if (!decoded.empty()) return decoded;
                    }
                    return value;
                };

                auto buildCallMessage = [&](const std::string& payload_type, std::string& out_decoded) {
                    std::string result = "{\"type\":\"" + payload_type + "\"";
                    result += ",\"token\":\"" + JsonParser::escapeJson(token) + "\"";
                    result += ",\"target_user_id\":\"" + JsonParser::escapeJson(target_user_id) + "\"";
                    result += ",\"receiver_id\":\"" + JsonParser::escapeJson(target_user_id) + "\"";
                    result += ",\"from_user_id\":\"" + JsonParser::escapeJson(user_id) + "\"";
                    result += ",\"from_username\":\"" + JsonParser::escapeJson(from_username) + "\"";
                    result += ",\"call_type\":\"" + JsonParser::escapeJson(call_type) + "\"";

                    if (payload_type == "call_offer" && data.count("offer")) {
                        std::string decoded = decodePayload(data.at("offer"), "offer_encoding");
                        out_decoded = decoded;
                        result += ",\"offer\":\"" + JsonParser::escapeJson(decoded) + "\"";
                    } else if (payload_type == "call_answer" && data.count("answer")) {
                        std::string decoded = decodePayload(data.at("answer"), "answer_encoding");
                        out_decoded = decoded;
                        result += ",\"answer\":\"" + JsonParser::escapeJson(decoded) + "\"";
                    } else if (payload_type == "call_ice_candidate" && data.count("candidate")) {
                        std::string decoded = decodePayload(data.at("candidate"), "candidate_encoding");
                        out_decoded = decoded;
                        result += ",\"candidate\":\"" + JsonParser::escapeJson(decoded) + "\"";
                    }

                    result += "}";
                    return result;
                };

                std::string decoded_payload;
                std::string forwarded_json = buildCallMessage(type, decoded_payload);

                // Сохраняем сигналинг, чтобы callee мог забрать даже если офлайн
                if (request_handler_ && !decoded_payload.empty()) {
                    if (type == "call_offer") {
                        request_handler_->storeCallOffer(user_id, target_user_id, decoded_payload);
                    } else if (type == "call_answer") {
                        // answer отправляет callee -> сохраняем под ключом caller_callee
                        request_handler_->storeCallAnswer(target_user_id, user_id, decoded_payload);
                    } else if (type == "call_ice_candidate") {
                        request_handler_->storeCallIce(user_id, target_user_id, decoded_payload);
                    }
                }

                Logger::getInstance().info("Forwarding " + type + " from user " + user_id + " to user " + target_user_id);
                Logger::getInstance().info("Forwarded JSON: " + forwarded_json);
                sendToUser(target_user_id, forwarded_json);
                
                // Отправляем подтверждение отправителю
                std::string response = "{\"success\":true,\"type\":\"" + type + "_sent\"}";
                ws->async_write(net::buffer(response),
                    [](beast::error_code, std::size_t) {});
            } else {
                // Если нет target_user_id, просто логируем ошибку
                Logger::getInstance().warning("No target_user_id in " + type + " message from user " + user_id);
            }
        } else if (type == "call_media_state" || type == "group_call_media_state") {
            // Обрабатываем состояние медиа (микрофон/камера выкл/вкл)
            std::string token = data["token"];
            if (token == kSessionTokenPlaceholder) {
                token.clear();
            }
            std::string user_id = token.empty() ? "" : auth_manager_->getUserIdFromToken(token);
            if (user_id.empty()) {
                user_id = getWebSocketUserId(ws);
            }
            
            if (user_id.empty()) {
                return;
            }
            
            std::string media_type = data.count("media_type") ? data["media_type"] : "";
            std::string enabled = data.count("enabled") ? data["enabled"] : "true";
            
            // Получаем имя отправителя
            std::string from_username = "Пользователь";
            try {
                auto user = db_manager_->getUserById(user_id);
                if (!user.id.empty()) {
                    from_username = user.username;
                }
            } catch (...) {}
            
            if (type == "call_media_state") {
                // Одиночный звонок - пересылаем собеседнику
                std::string target_user_id = data.count("target_user_id") ? data["target_user_id"] : "";
                if (!target_user_id.empty()) {
                    std::string payload = "{\"type\":\"call_media_state\","
                        "\"from_user_id\":\"" + JsonParser::escapeJson(user_id) + "\","
                        "\"from_username\":\"" + JsonParser::escapeJson(from_username) + "\","
                        "\"media_type\":\"" + JsonParser::escapeJson(media_type) + "\","
                        "\"enabled\":" + enabled + "}";
                    sendToUser(target_user_id, payload);
                    Logger::getInstance().debug("Forwarded call_media_state from " + user_id + " to " + target_user_id);
                }
            } else {
                // Групповой звонок - пересылаем всем участникам группы
                std::string group_id = data.count("group_id") ? data["group_id"] : "";
                if (!group_id.empty()) {
                    std::string payload = "{\"type\":\"group_call_media_state\","
                        "\"from_user_id\":\"" + JsonParser::escapeJson(user_id) + "\","
                        "\"from_username\":\"" + JsonParser::escapeJson(from_username) + "\","
                        "\"group_id\":\"" + JsonParser::escapeJson(group_id) + "\","
                        "\"media_type\":\"" + JsonParser::escapeJson(media_type) + "\","
                        "\"enabled\":" + enabled + "}";
                    // Рассылаем всем участникам группы
                    try {
                        auto members = db_manager_->getGroupMembers(group_id);
                        for (const auto& member : members) {
                            if (member.user_id != user_id) {
                                sendToUser(member.user_id, payload);
                            }
                        }
                        Logger::getInstance().debug("Broadcasted group_call_media_state from " + user_id + " to group " + group_id);
                    } catch (...) {
                        Logger::getInstance().warning("Failed to get group members for media state broadcast");
                    }
                }
            }
        } else if (type == "typing") {
            std::string token = data["token"];
            if (token == kSessionTokenPlaceholder) {
                token.clear();
            }
            std::string user_id = token.empty() ? "" : auth_manager_->getUserIdFromToken(token);
            if (user_id.empty()) {
                user_id = getWebSocketUserId(ws);
            }

            if (user_id.empty()) {
                std::string error_response = "{\"type\":\"error\",\"error\":\"Not authenticated\"}";
                ws->async_write(net::buffer(error_response),
                    [](beast::error_code, std::size_t) {});
                return;
            }

            std::string chat_type = data.count("chat_type") ? data["chat_type"] : "chat";
            std::string chat_id = data.count("chat_id") ? data["chat_id"] : "";
            std::string is_typing = data.count("is_typing") ? data["is_typing"] : "1";

            if (chat_id.empty()) {
                Logger::getInstance().warning("No chat_id in typing message from user " + user_id);
                return;
            }

            std::string normalized_type = chat_type;
            if (normalized_type == "direct" || normalized_type == "dm") {
                normalized_type = "chat";
            }

            std::string from_username = "Пользователь";
            try {
                auto user = db_manager_->getUserById(user_id);
                if (!user.id.empty()) {
                    from_username = user.username;
                }
            } catch (...) {
                Logger::getInstance().warning("Could not get username for user: " + user_id);
            }

            std::string payload = "{\"type\":\"typing\","
                "\"chat_type\":\"" + JsonParser::escapeJson(normalized_type) + "\","
                "\"chat_id\":\"" + JsonParser::escapeJson(chat_id) + "\","
                "\"from_user_id\":\"" + JsonParser::escapeJson(user_id) + "\","
                "\"from_username\":\"" + JsonParser::escapeJson(from_username) + "\","
                "\"is_typing\":\"" + JsonParser::escapeJson(is_typing) + "\"}";

            if (normalized_type == "group") {
                auto members = db_manager_->getGroupMembers(chat_id);
                for (const auto& member : members) {
                    if (member.user_id.empty() || member.user_id == user_id) continue;
                    sendToUser(member.user_id, payload);
                }
            } else if (normalized_type == "channel") {
                auto subscribers = db_manager_->getChannelSubscriberIds(chat_id);
                for (const auto& uid : subscribers) {
                    if (uid.empty() || uid == user_id) continue;
                    sendToUser(uid, payload);
                }
            } else {
                std::string target_user_id = chat_id;
                if (!target_user_id.empty() && target_user_id != user_id) {
                    sendToUser(target_user_id, payload);
                }
            }
        } else if (type == "message_delivered" || type == "message_read") {
            std::string token = data["token"];
            if (token == kSessionTokenPlaceholder) {
                token.clear();
            }
            std::string user_id = token.empty() ? "" : auth_manager_->getUserIdFromToken(token);
            if (user_id.empty()) {
                user_id = getWebSocketUserId(ws);
            }
            if (user_id.empty()) {
                std::string error_response = "{\"type\":\"error\",\"error\":\"Not authenticated\"}";
                ws->async_write(net::buffer(error_response),
                    [](beast::error_code, std::size_t) {});
                return;
            }

            std::string message_id = data.count("message_id") ? data["message_id"] : "";
            if (message_id.empty()) {
                Logger::getInstance().warning("No message_id in " + type + " message from user " + user_id);
                return;
            }

            Message msg = db_manager_->getMessageById(message_id);
            if (msg.id.empty()) {
                Logger::getInstance().warning("Message not found for " + type + " message_id " + message_id);
                return;
            }
            if (!msg.receiver_id.empty() && msg.receiver_id != user_id) {
                Logger::getInstance().warning("User " + user_id + " is not receiver for " + message_id);
                return;
            }

            if (type == "message_read" && !msg.sender_id.empty()) {
                db_manager_->markMessagesAsRead(user_id, msg.sender_id);
            } else if (type == "message_delivered") {
                db_manager_->markMessageDeliveredById(message_id);
            }

            bool allow_read_receipts = true;
            if (type == "message_read") {
                UserPrivacySettings privacy;
                if (db_manager_->getUserPrivacy(user_id, privacy)) {
                    allow_read_receipts = privacy.send_read_receipts;
                }
            }

            if (!msg.sender_id.empty() && (type != "message_read" || allow_read_receipts)) {
                std::string payload = "{\"type\":\"" + JsonParser::escapeJson(type) + "\","
                    "\"message_id\":\"" + JsonParser::escapeJson(message_id) + "\","
                    "\"chat_id\":\"" + JsonParser::escapeJson(user_id) + "\","
                    "\"from_user_id\":\"" + JsonParser::escapeJson(user_id) + "\"}";
                sendToUser(msg.sender_id, payload);
            }
        } else if (type == "group_call_offer" || type == "group_call_answer" || 
                   type == "group_call_ice_candidate" || type == "group_call_end" || 
                   type == "group_call_notification") {
            // Обрабатываем групповые звонки через WebSocket
            std::string token = data["token"];
            if (token == kSessionTokenPlaceholder) {
                token.clear();
            }
            std::string user_id = token.empty() ? "" : auth_manager_->getUserIdFromToken(token);
            if (user_id.empty()) {
                user_id = getWebSocketUserId(ws);
            }
            
            Logger::getInstance().info("Processing " + type + " from user " + user_id);
            
            if (user_id.empty()) {
                std::string error_response = "{\"type\":\"error\",\"error\":\"Not authenticated\"}";
                ws->async_write(net::buffer(error_response),
                    [](beast::error_code, std::size_t) {});
                return;
            }
            
            // Получаем group_id из сообщения
            std::string group_id;
            if (data.count("group_id")) {
                group_id = data["group_id"];
            }
            
            if (group_id.empty()) {
                Logger::getInstance().warning("No group_id in " + type + " message from user " + user_id);
                return;
            }
            
            // Получаем имя пользователя
            std::string from_username = "Пользователь";
            try {
                auto user = db_manager_->getUserById(user_id);
                if (!user.id.empty()) {
                    from_username = user.username;
                }
            } catch (...) {
                Logger::getInstance().warning("Could not get username for user: " + user_id);
            }
            
            if (type == "group_call_notification") {
                // Отправляем уведомление всем участникам группы
                auto members = db_manager_->getGroupMembers(group_id);
                
                // Получаем название группы для уведомления
                std::string group_name = "Группа";
                try {
                    auto group = db_manager_->getGroupById(group_id);
                    if (!group.id.empty()) {
                        group_name = group.name;
                    }
                } catch (...) {
                    Logger::getInstance().warning("Could not get group name for group: " + group_id);
                }
                
                std::string call_type = data.count("call_type") ? data["call_type"] : "video";
                
                // Отправляем всем участникам группы
                for (const auto& member : members) {
                    if (member.user_id != user_id) { // Не отправляем себе
                        // Формируем JSON для каждого участника отдельно
                        std::string notification_json = "{\"type\":\"group_call_notification\","
                            "\"group_id\":\"" + JsonParser::escapeJson(group_id) + "\","
                            "\"group_name\":\"" + JsonParser::escapeJson(group_name) + "\","
                            "\"from_user_id\":\"" + user_id + "\","
                            "\"from_username\":\"" + JsonParser::escapeJson(from_username) + "\","
                            "\"call_type\":\"" + call_type + "\","
                            "\"members\":[";
                        
                        bool first = true;
                        for (const auto& m : members) {
                            if (!first) notification_json += ",";
                            first = false;
                            // Отправляем как строку, чтобы фронтенд мог правильно сравнить
                            notification_json += "\"" + JsonParser::escapeJson(m.user_id) + "\"";
                        }
                        notification_json += "]}";
                        
                        Logger::getInstance().info("Sending group call notification to user: " + member.user_id);
                        sendToUser(member.user_id, notification_json);
                    }
                }
            } else {
                // Для остальных типов сообщений нужен target_user_id
                std::string target_user_id;
                if (data.count("target_user_id")) {
                    target_user_id = data["target_user_id"];
                }
                
                if (!target_user_id.empty()) {
                    // Добавляем from_user_id и group_id в сообщение
                    std::string forwarded_json = message;
                    if (forwarded_json.find("\"from_user_id\"") == std::string::npos) {
                        size_t last_brace = forwarded_json.find_last_of('}');
                        if (last_brace != std::string::npos) {
                            std::string additional_fields = ",\"from_user_id\":\"" + user_id + 
                                                           "\",\"from_username\":\"" + 
                                                           JsonParser::escapeJson(from_username) + 
                                                           "\",\"group_id\":\"" + 
                                                           JsonParser::escapeJson(group_id) + "\"";
                            forwarded_json.insert(last_brace, additional_fields);
                        }
                    }
                    
                    Logger::getInstance().info("Forwarding " + type + " from user " + user_id + " to user " + target_user_id + " in group " + group_id);
                    sendToUser(target_user_id, forwarded_json);
                } else if (type == "group_call_end") {
                    // Отправляем всем участникам группы о завершении звонка
                    auto members = db_manager_->getGroupMembers(group_id);
                    std::string end_json = "{\"type\":\"group_call_end\","
                        "\"group_id\":\"" + JsonParser::escapeJson(group_id) + "\","
                        "\"user_id\":\"" + user_id + "\","
                        "\"from_user_id\":\"" + user_id + "\"}";
                    
                    for (const auto& member : members) {
                        if (member.user_id != user_id) {
                            sendToUser(member.user_id, end_json);
                        }
                    }
                }
            }
            
            // Отправляем подтверждение отправителю
            std::string response = "{\"success\":true,\"type\":\"" + type + "_sent\"}";
            ws->async_write(net::buffer(response),
                [](beast::error_code, std::size_t) {});
        }
    } catch (const std::exception& e) {
        Logger::getInstance().error("Error processing WebSocket message: " + std::string(e.what()));
    } catch (...) {
        Logger::getInstance().error("Unknown error processing WebSocket message");
    }
}

void HttpServer::registerWebSocketConnection(const std::string& user_id, 
                                             std::shared_ptr<websocket::stream<beast::tcp_stream>> ws) {
    {
        std::lock_guard<std::mutex> lock(ws_connections_mutex_);
        ws_connections_[user_id] = ws;
    }
    {
        std::lock_guard<std::mutex> lock(ws_user_ids_mutex_);
        ws_user_ids_[ws.get()] = user_id;
    }
    Logger::getInstance().info("========== WebSocket connection registered ==========");
    Logger::getInstance().info("User ID: " + user_id);
    Logger::getInstance().info("Total connections now: " + std::to_string(ws_connections_.size()));
    
    // Выводим список всех подключенных пользователей
    std::string connected_users = "All connected users: ";
    for (const auto& pair : ws_connections_) {
        connected_users += pair.first + " ";
    }
    Logger::getInstance().info(connected_users);
}

void HttpServer::unregisterWebSocketConnection(const std::string& user_id) {
    {
        std::lock_guard<std::mutex> lock(ws_connections_mutex_);
        ws_connections_.erase(user_id);
    }
    {
        std::lock_guard<std::mutex> lock(ws_user_ids_mutex_);
        for (auto it = ws_user_ids_.begin(); it != ws_user_ids_.end();) {
            if (it->second == user_id) {
                it = ws_user_ids_.erase(it);
            } else {
                ++it;
            }
        }
    }
    Logger::getInstance().info("WebSocket connection unregistered for user: " + user_id);
}

std::string HttpServer::getWebSocketUserId(std::shared_ptr<websocket::stream<beast::tcp_stream>> ws) {
    std::lock_guard<std::mutex> lock(ws_user_ids_mutex_);
    auto it = ws_user_ids_.find(ws.get());
    if (it == ws_user_ids_.end()) {
        return "";
    }
    return it->second;
}

void HttpServer::sendToUser(const std::string& user_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(ws_connections_mutex_);
    
    // Логируем все подключенные пользователи для отладки
    Logger::getInstance().info("sendToUser called for user: " + user_id);
    Logger::getInstance().info("Total WebSocket connections: " + std::to_string(ws_connections_.size()));
    
    // Выводим список всех подключенных пользователей
    std::string connected_users = "Connected users: ";
    for (const auto& pair : ws_connections_) {
        connected_users += pair.first + " ";
    }
    Logger::getInstance().info(connected_users);
    
    auto it = ws_connections_.find(user_id);
    if (it != ws_connections_.end()) {
        auto ws = it->second.lock();
        if (ws) {
            Logger::getInstance().info("Sending WebSocket message to user: " + user_id + ", message: " + message);
            ws->async_write(net::buffer(message),
                [user_id, message](beast::error_code ec, std::size_t) {
                    if (ec) {
                        Logger::getInstance().error("Error sending WebSocket message to user " + user_id + ": " + ec.message());
                    } else {
                        Logger::getInstance().info("WebSocket message sent successfully to user: " + user_id);
                    }
                });
        } else {
            // Соединение закрыто, удаляем из списка
            Logger::getInstance().warning("WebSocket connection expired for user: " + user_id);
            ws_connections_.erase(it);
        }
    } else {
        Logger::getInstance().warning("No WebSocket connection found for user: " + user_id + " (total connections: " + std::to_string(ws_connections_.size()) + ")");
    }
}

} // namespace xipher
