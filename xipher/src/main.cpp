#include "server/http_server.hpp"
#include "utils/logger.hpp"
#include <iostream>
#include <csignal>
#include <cstdlib>

xipher::HttpServer* g_server = nullptr;

void signalHandler(int signal) {
    if (g_server) {
        std::cout << "\nShutting down server..." << std::endl;
        g_server->stop();
    }
    exit(signal);
}

int main(int argc, char* argv[]) {
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Setup logging
    xipher::Logger::getInstance().setLogFile("/var/log/xipher/server.log");
    xipher::Logger::getInstance().info("Starting Xipher Server...");
    
    // Server configuration
    // Use 0.0.0.0 to accept connections from all interfaces (for api.xipher.pro)
    std::string address = "0.0.0.0";
    unsigned short port = 8080;  // Standard HTTP port for API
    
    // Allow port override via command line argument
    if (argc > 1) {
        port = static_cast<unsigned short>(std::stoi(argv[1]));
    }
    
    // Allow address override via environment variable
    const char* env_address = std::getenv("XIPHER_SERVER_ADDRESS");
    if (env_address) {
        address = std::string(env_address);
    }
    
    xipher::Logger::getInstance().info("Server will listen on " + address + ":" + std::to_string(port));
    
    // Create and start server
    xipher::HttpServer server(address, port);
    g_server = &server;
    
    if (!server.start()) {
        xipher::Logger::getInstance().error("Failed to start server");
        return 1;
    }
    
    return 0;
}

