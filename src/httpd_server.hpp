//
//  httpd_server.hpp
//  gomoku-httpd - HTTP server implementation
//
//  Modern C++23 HTTP server with thread pool and JSON API
//

#pragma once

#include <string>
#include <memory>
#include <thread>
#include <vector>
#include <atomic>
#include <expected>

#include "httplib.h"
#include "json.hpp"
#include "httpd_cli.hpp"
#include "httpd_game_api.hpp"

namespace gomoku::httpd {

using json = nlohmann::json;

class HttpServer {
public:
    explicit HttpServer(const HttpDaemonConfig& config);
    ~HttpServer();
    
    bool start();
    void stop();
    
    bool is_running() const { return running_.load(); }
    
private:
    void setup_routes();
    void setup_middleware();
    
    // Route handlers
    void handle_status(const httplib::Request& req, httplib::Response& res);
    void handle_move(const httplib::Request& req, httplib::Response& res);
    void handle_schema(const httplib::Request& req, httplib::Response& res);
    void handle_not_found(const httplib::Request& req, httplib::Response& res);
    
    // Utility methods
    json get_system_metrics() const;
    std::expected<json, std::string> validate_move_request(const json& request) const;
    json create_error_response(const std::string& error, int code = 400) const;
    
    HttpDaemonConfig config_;
    std::unique_ptr<httplib::Server> server_;
    std::unique_ptr<GameAPI> game_api_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
};

} // namespace gomoku::httpd