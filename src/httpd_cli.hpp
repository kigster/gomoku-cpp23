//
//  httpd_cli.hpp
//  gomoku-httpd - Command-line interface for HTTP daemon
//
//  Modern C++23 CLI parsing with std::expected and concepts
//

#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <span>
#include <thread>

namespace gomoku::httpd {

enum class CliError {
    InvalidArgument,
    MissingValue,
    InvalidPort,
    InvalidDepth,
    InvalidThreads,
    HelpRequested
};

struct HttpDaemonConfig {
    std::string host = "0.0.0.0";
    int port = 5500;
    int threads = []() { 
        auto cores = std::thread::hardware_concurrency(); 
        return cores > 1 ? cores - 1 : 1; 
    }();
    int depth = 6;
    bool daemon_mode = false;
    bool foreground_mode = false;
    bool verbose = false;
};

constexpr bool is_valid_port(int port) noexcept {
    return port >= 1 && port <= 65535;
}

constexpr bool is_valid_depth(int depth) noexcept {
    return depth >= 1 && depth <= 10;
}

constexpr bool is_valid_threads(int threads) noexcept {
    return threads >= 1 && threads <= 64;
}

std::expected<HttpDaemonConfig, CliError> parse_command_line(int argc, char* argv[]);

void print_help(std::string_view program_name);
void print_version();

const char* cli_error_to_string(CliError error);

} // namespace gomoku::httpd