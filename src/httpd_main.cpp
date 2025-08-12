//
//  httpd_main.cpp
//  gomoku-httpd - HTTP daemon main entry point
//
//  Modern C++23 HTTP server for stateless Gomoku AI API
//

#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstdlib>
#include <filesystem>
#include <format>

#include "httpd_server.hpp"
#include "httpd_cli.hpp"

namespace gomoku::httpd {

constexpr int DEFAULT_PORT = 5500;
constexpr std::string_view DEFAULT_HOST = "0.0.0.0";
const int DEFAULT_THREADS = std::thread::hardware_concurrency() > 1 ? 
                            std::thread::hardware_concurrency() - 1 : 1;
constexpr int DEFAULT_DEPTH = 6;

volatile sig_atomic_t shutdown_requested = 0;

void signal_handler(int signal) {
    shutdown_requested = 1;
    std::cout << std::format("\nReceived signal {}, shutting down gracefully...\n", signal);
}

void setup_signal_handlers() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
}

bool daemonize() {
    pid_t pid = fork();
    
    if (pid < 0) {
        std::cerr << "Fork failed\n";
        return false;
    }
    
    if (pid > 0) {
        // Parent process exits
        exit(0);
    }
    
    // Child process continues
    if (setsid() < 0) {
        std::cerr << "Failed to create new session\n";
        return false;
    }
    
    // Fork again to ensure we can't acquire a controlling terminal
    pid = fork();
    if (pid < 0) {
        std::cerr << "Second fork failed\n";
        return false;
    }
    
    if (pid > 0) {
        // First child exits
        exit(0);
    }
    
    // Change working directory to root
    if (chdir("/") < 0) {
        std::cerr << "Failed to change directory to /\n";
        return false;
    }
    
    // Close standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    return true;
}

} // namespace gomoku::httpd

using namespace gomoku::httpd;

int main(int argc, char* argv[]) {
    try {
        auto config = parse_command_line(argc, argv);
        if (!config) {
            return 1;
        }
        
        if (config->daemon_mode && !config->foreground_mode) {
            std::cout << std::format("Starting gomoku-httpd in daemon mode on {}:{}\n", 
                                   config->host, config->port);
            
            if (!daemonize()) {
                std::cerr << "Failed to daemonize\n";
                return 1;
            }
        } else {
            std::cout << std::format("Starting gomoku-httpd in foreground on {}:{} with {} threads, depth {}\n", 
                                   config->host, config->port, config->threads, config->depth);
        }
        
        setup_signal_handlers();
        
        // Create and start the HTTP server
        HttpServer server(*config);
        if (!server.start()) {
            std::cerr << "Failed to start HTTP server\n";
            return 1;
        }
        
        // Main event loop
        while (!shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "Shutting down server...\n";
        server.stop();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << std::format("Error: {}\n", e.what());
        return 1;
    }
}