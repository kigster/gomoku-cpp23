//
//  httpd_server.cpp
//  gomoku-httpd - HTTP server implementation
//
//  Modern C++23 HTTP server with comprehensive API endpoints
//

#include "httpd_server.hpp"
#include <iostream>
#include <format>
#include <fstream>
#include <chrono>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <unistd.h>

namespace gomoku::httpd {

HttpServer::HttpServer(const HttpDaemonConfig& config) 
    : config_(config), server_(std::make_unique<httplib::Server>()),
      game_api_(std::make_unique<GameAPI>(config.depth)) {
    
    setup_middleware();
    setup_routes();
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    if (running_.load()) {
        return true;
    }
    
    server_thread_ = std::thread([this]() {
        try {
            running_.store(true);
            
            if (config_.verbose) {
                server_->set_logger([](const httplib::Request& req, const httplib::Response& res) {
                    std::cout << std::format("{} {} -> {} ({}ms)\n", 
                                           req.method, req.path, res.status,
                                           /* timing would go here */0);
                });
            }
            
            bool success = server_->listen(config_.host, config_.port);
            if (!success && running_.load()) {
                std::cerr << std::format("Failed to bind to {}:{}\n", config_.host, config_.port);
            }
            
        } catch (const std::exception& e) {
            std::cerr << std::format("Server error: {}\n", e.what());
        }
        
        running_.store(false);
    });
    
    // Give the server a moment to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    return running_.load();
}

void HttpServer::stop() {
    if (running_.load()) {
        running_.store(false);
        server_->stop();
        
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
}

void HttpServer::setup_middleware() {
    // CORS headers
    server_->set_pre_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });
    
    // Content-Type validation for POST requests
    server_->set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        if (req.method == "POST") {
            auto content_type = req.get_header_value("Content-Type");
            if (content_type.find("application/json") == std::string::npos) {
                res.status = 400;
                res.set_content(R"({"error": "Content-Type must be application/json"})", 
                               "application/json");
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
}

void HttpServer::setup_routes() {
    server_->Get("/ai/v1/status", [this](const httplib::Request& req, httplib::Response& res) {
        handle_status(req, res);
    });
    
    server_->Post("/ai/v1/move", [this](const httplib::Request& req, httplib::Response& res) {
        handle_move(req, res);
    });
    
    server_->Get("/gomoku.schema.json", [this](const httplib::Request& req, httplib::Response& res) {
        handle_schema(req, res);
    });
    
    server_->Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"status": "ok", "service": "gomoku-httpd"})", "application/json");
    });
    
    server_->set_error_handler([this](const httplib::Request& req, httplib::Response& res) {
        handle_not_found(req, res);
    });
}

void HttpServer::handle_status(const httplib::Request&, httplib::Response& res) {
    try {
        json status_response;
        status_response["status"] = "healthy";
        status_response["service"] = "gomoku-httpd";
        status_response["version"] = gomoku::GAME_VERSION;
        status_response["config"]["depth"] = config_.depth;
        status_response["config"]["threads"] = config_.threads;
        status_response["metrics"] = get_system_metrics();
        
        res.set_content(status_response.dump(2), "application/json");
        
    } catch (const std::exception& e) {
        res.status = 500;
        auto error_response = create_error_response(
            std::format("Failed to get system metrics: {}", e.what()), 500);
        res.set_content(error_response.dump(), "application/json");
    }
}

void HttpServer::handle_move(const httplib::Request& req, httplib::Response& res) {
    try {
        json request_json = json::parse(req.body);
        
        // Validate request against schema
        auto validation_result = validate_move_request(request_json);
        if (!validation_result) {
            res.status = 501;
            auto error_response = create_error_response(
                std::format("Schema validation failed: {}", validation_result.error()), 501);
            res.set_content(error_response.dump(), "application/json");
            return;
        }
        
        auto move_result = game_api_->process_move_request(request_json);
        if (!move_result) {
            res.status = 400;
            auto error_response = create_error_response("Invalid move request", 400);
            res.set_content(error_response.dump(), "application/json");
            return;
        }
        
        json response_json;
        response_json["game_id"] = move_result->game_id;
        response_json["game_status"] = move_result->game_status;
        response_json["board_state"] = move_result->board_state;
        response_json["move_history"] = move_result->move_history;
        response_json["latest_move"] = move_result->move;
        response_json["ai_metrics"]["positions_evaluated"] = move_result->positions_evaluated;
        response_json["ai_metrics"]["move_time_ms"] = move_result->move_time_ms;
        
        res.set_content(response_json.dump(2), "application/json");
        
    } catch (const json::parse_error& e) {
        res.status = 400;
        auto error_response = create_error_response(
            std::format("Invalid JSON: {}", e.what()), 400);
        res.set_content(error_response.dump(), "application/json");
        
    } catch (const std::exception& e) {
        res.status = 500;
        auto error_response = create_error_response(
            std::format("Server error: {}", e.what()), 500);
        res.set_content(error_response.dump(), "application/json");
    }
}

void HttpServer::handle_schema(const httplib::Request&, httplib::Response& res) {
    try {
        std::ifstream schema_file("schema/gomoku.schema.json");
        if (!schema_file.is_open()) {
            res.status = 404;
            auto error_response = create_error_response("Schema file not found", 404);
            res.set_content(error_response.dump(), "application/json");
            return;
        }
        
        json schema_json;
        schema_file >> schema_json;
        
        res.set_content(schema_json.dump(2), "application/json");
        
    } catch (const std::exception& e) {
        res.status = 500;
        auto error_response = create_error_response(
            std::format("Failed to load schema: {}", e.what()), 500);
        res.set_content(error_response.dump(), "application/json");
    }
}

void HttpServer::handle_not_found(const httplib::Request& req, httplib::Response& res) {
    res.status = 404;
    auto error_response = create_error_response(
        std::format("Endpoint not found: {} {}", req.method, req.path), 404);
    res.set_content(error_response.dump(), "application/json");
}

json HttpServer::get_system_metrics() const {
    json metrics;
    
    try {
        // Get load averages
        double loadavg[3];
        if (getloadavg(loadavg, 3) != -1) {
            metrics["load_average"]["1min"] = loadavg[0];
            metrics["load_average"]["5min"] = loadavg[1];
            metrics["load_average"]["15min"] = loadavg[2];
        }
        
        // Get memory info (macOS specific)
        vm_size_t page_size;
        vm_statistics64_data_t vm_stat;
        mach_port_t mach_port = mach_host_self();
        mach_msg_type_number_t count = sizeof(vm_stat) / sizeof(natural_t);
        
        if (host_page_size(mach_port, &page_size) == KERN_SUCCESS &&
            host_statistics64(mach_port, HOST_VM_INFO, 
                            (host_info64_t)&vm_stat, &count) == KERN_SUCCESS) {
            
            uint64_t total_memory = (vm_stat.free_count + vm_stat.active_count + 
                                   vm_stat.inactive_count + vm_stat.wire_count) * page_size;
            uint64_t free_memory = vm_stat.free_count * page_size;
            
            metrics["memory"]["total_bytes"] = total_memory;
            metrics["memory"]["free_bytes"] = free_memory;
            metrics["memory"]["used_bytes"] = total_memory - free_memory;
            metrics["memory"]["free_percentage"] = 
                total_memory > 0 ? (double(free_memory) / double(total_memory)) * 100.0 : 0.0;
        }
        
        // Get process info
        metrics["process"]["pid"] = getpid();
        metrics["process"]["threads_configured"] = config_.threads;
        metrics["process"]["ai_depth"] = config_.depth;
        
        // Get CPU count
        metrics["system"]["cpu_cores"] = std::thread::hardware_concurrency();
        
    } catch (const std::exception& e) {
        metrics["error"] = std::format("Failed to collect metrics: {}", e.what());
    }
    
    return metrics;
}

std::expected<json, std::string> HttpServer::validate_move_request(const json& request) const {
    try {
        // Check required top-level fields
        if (!request.contains("version") || request["version"] != "1.0") {
            return std::unexpected("Missing or invalid 'version' field (must be '1.0')");
        }
        
        if (!request.contains("game") || !request["game"].is_object()) {
            return std::unexpected("Missing or invalid 'game' object");
        }
        
        if (!request.contains("players") || !request["players"].is_object()) {
            return std::unexpected("Missing or invalid 'players' object");
        }
        
        if (!request.contains("moves") || !request["moves"].is_array()) {
            return std::unexpected("Missing or invalid 'moves' array");
        }
        
        if (!request.contains("current_player") || !request["current_player"].is_string()) {
            return std::unexpected("Missing or invalid 'current_player' field");
        }
        
        // board_state is now optional (can be reconstructed from moves)
        
        // Validate game object
        const auto& game = request["game"];
        if (!game.contains("id") || !game["id"].is_string()) {
            return std::unexpected("Missing or invalid game.id");
        }
        
        if (!game.contains("board_size") || !game["board_size"].is_number_integer()) {
            return std::unexpected("Missing or invalid game.board_size");
        }
        
        int board_size = game["board_size"];
        if (board_size != 15 && board_size != 19) {
            return std::unexpected("game.board_size must be 15 or 19");
        }
        
        // Validate current_player
        std::string current_player = request["current_player"];
        if (current_player != "x" && current_player != "o") {
            return std::unexpected("current_player must be 'x' or 'o'");
        }
        
        // Validate board_state format if provided (it's optional)
        if (request.contains("board_state")) {
            if (!request["board_state"].is_array()) {
                return std::unexpected("board_state must be an array if provided");
            }
            
            const auto& board_state = request["board_state"];
            if (static_cast<int>(board_state.size()) != board_size) {
                return std::unexpected(std::format("board_state must have exactly {} rows for board size {}", 
                                                 board_size, board_size));
            }
            
            // Validate each row format
            for (size_t i = 0; i < board_state.size(); ++i) {
                if (!board_state[i].is_string()) {
                    return std::unexpected(std::format("board_state row {} must be a string", i));
                }
                
                std::string row = board_state[i];
                size_t expected_length = board_size * 3;  // Each cell is 3 characters
                if (row.length() != expected_length) {
                    return std::unexpected(std::format("board_state row {} must be exactly {} characters long", 
                                                     i, expected_length));
                }
                
                // Validate row pattern: each position should be " • ", " x ", or " o "
                for (int j = 0; j < board_size; ++j) {
                    size_t pos = j * 3;
                    if (pos + 2 >= row.length()) {
                        return std::unexpected(std::format("board_state row {} is malformed at position {}", i, j));
                    }
                    
                    std::string cell = row.substr(pos, 3);
                    if (cell != " • " && cell != " x " && cell != " o ") {
                        return std::unexpected(std::format("board_state row {} position {} has invalid cell '{}' (must be ' • ', ' x ', or ' o ')", 
                                                         i, j, cell));
                    }
                }
            }
        }
        
        return request;  // Validation successful
        
    } catch (const json::exception& e) {
        return std::unexpected(std::format("JSON parsing error: {}", e.what()));
    } catch (const std::exception& e) {
        return std::unexpected(std::format("Validation error: {}", e.what()));
    }
}

json HttpServer::create_error_response(const std::string& error, int code) const {
    json error_response;
    error_response["error"] = error;
    error_response["code"] = code;
    error_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return error_response;
}

} // namespace gomoku::httpd