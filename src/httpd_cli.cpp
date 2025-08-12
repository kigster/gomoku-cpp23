//
//  httpd_cli.cpp
//  gomoku-httpd - Command-line interface implementation
//
//  Modern C++23 CLI parsing with std::expected
//

#include "httpd_cli.hpp"
#include "gomoku.hpp"
#include <iostream>
#include <format>
#include <charconv>
#include <algorithm>

namespace gomoku::httpd {

const char* cli_error_to_string(CliError error) {
    switch (error) {
        case CliError::InvalidArgument:
            return "Invalid argument";
        case CliError::MissingValue:
            return "Missing value for argument";
        case CliError::InvalidPort:
            return "Invalid port number (must be 1-65535)";
        case CliError::InvalidDepth:
            return "Invalid depth (must be 1-10)";
        case CliError::InvalidThreads:
            return "Invalid thread count (must be 1-64)";
        case CliError::HelpRequested:
            return "Help requested";
        default:
            return "Unknown error";
    }
}

std::expected<int, CliError> parse_int(std::string_view str) {
    int value{};
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    
    if (ec != std::errc{} || ptr != str.data() + str.size()) {
        return std::unexpected(CliError::InvalidArgument);
    }
    
    return value;
}

void print_help(std::string_view program_name) {
    std::cout << std::format(R"(
gomoku-httpd - Gomoku AI HTTP daemon

USAGE:
    {} [OPTIONS]

OPTIONS:
    -h, --help                Show this help message
    -v, --version            Show version information
    -p, --port <PORT>        TCP port to bind to (default: 5500)
    -H, --host <HOST>        IP address to bind to (default: 0.0.0.0)
    -t, --threads <COUNT>    Number of worker threads (default: CPU cores - 1)
    -d, --depth <DEPTH>      AI search depth (default: 6, range: 1-10)
    --daemon                 Run as daemon (detach from TTY)
    --foreground             Run in foreground (for testing, default behavior)
    --verbose                Enable verbose logging

ENDPOINTS:
    GET  /ai/v1/status       Server health and system metrics
    POST /ai/v1/move         Request AI move for game position
    GET  /gomoku.schema.json JSON schema for game state format

EXAMPLES:
    {} --port 8080 --threads 4 --depth 8
    {} --host 127.0.0.1 --daemon --verbose

)", program_name, program_name, program_name);
}

void print_version() {
    std::cout << std::format("gomoku-httpd version {}\n", gomoku::GAME_VERSION);
    std::cout << "C++23 Gomoku AI HTTP daemon\n";
    std::cout << "Built with modern C++23 features\n";
}

std::expected<HttpDaemonConfig, CliError> parse_command_line(int argc, char* argv[]) {
    HttpDaemonConfig config;
    
    std::span<char*> args(argv, argc);
    
    for (size_t i = 1; i < args.size(); ++i) {
        std::string_view arg = args[i];
        
        if (arg == "-h" || arg == "--help") {
            print_help(args[0]);
            return std::unexpected(CliError::HelpRequested);
        }
        
        if (arg == "-v" || arg == "--version") {
            print_version();
            return std::unexpected(CliError::HelpRequested);
        }
        
        if (arg == "--daemon") {
            config.daemon_mode = true;
            continue;
        }
        
        if (arg == "--foreground") {
            config.foreground_mode = true;
            continue;
        }
        
        if (arg == "--verbose") {
            config.verbose = true;
            continue;
        }
        
        // Arguments that require values
        if (i + 1 >= args.size()) {
            std::cerr << std::format("Error: {} requires a value\n", arg);
            return std::unexpected(CliError::MissingValue);
        }
        
        std::string_view value = args[i + 1];
        
        if (arg == "-p" || arg == "--port") {
            auto port_result = parse_int(value);
            if (!port_result) {
                std::cerr << std::format("Error: Invalid port value '{}'\n", value);
                return std::unexpected(CliError::InvalidPort);
            }
            
            if (!is_valid_port(*port_result)) {
                std::cerr << std::format("Error: Port {} out of range (1-65535)\n", *port_result);
                return std::unexpected(CliError::InvalidPort);
            }
            
            config.port = *port_result;
            ++i; // Skip the value argument
            continue;
        }
        
        if (arg == "-H" || arg == "--host") {
            config.host = value;
            ++i;
            continue;
        }
        
        if (arg == "-t" || arg == "--threads") {
            auto threads_result = parse_int(value);
            if (!threads_result) {
                std::cerr << std::format("Error: Invalid threads value '{}'\n", value);
                return std::unexpected(CliError::InvalidThreads);
            }
            
            if (!is_valid_threads(*threads_result)) {
                std::cerr << std::format("Error: Thread count {} out of range (1-64)\n", *threads_result);
                return std::unexpected(CliError::InvalidThreads);
            }
            
            config.threads = *threads_result;
            ++i;
            continue;
        }
        
        if (arg == "-d" || arg == "--depth") {
            auto depth_result = parse_int(value);
            if (!depth_result) {
                std::cerr << std::format("Error: Invalid depth value '{}'\n", value);
                return std::unexpected(CliError::InvalidDepth);
            }
            
            if (!is_valid_depth(*depth_result)) {
                std::cerr << std::format("Error: Depth {} out of range (1-10)\n", *depth_result);
                return std::unexpected(CliError::InvalidDepth);
            }
            
            config.depth = *depth_result;
            ++i;
            continue;
        }
        
        std::cerr << std::format("Error: Unknown argument '{}'\n", arg);
        return std::unexpected(CliError::InvalidArgument);
    }
    
    return config;
}

} // namespace gomoku::httpd