//
//  game_history.cpp
//  gomoku - Game history JSON logging implementation
//
//  Saves game state to JSON files compatible with HTTP daemon schema
//

#include "game_history.hpp"
#include "gomoku.hpp"
#include "ansi.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <random>
#include <cstdlib>
#include <cstring>

namespace gomoku {

const char* game_history_error_to_string(GameHistoryError error) {
    switch (error) {
        case GameHistoryError::DirectoryCreationFailed:
            return "Failed to create game_histories directory";
        case GameHistoryError::FileWriteFailed:
            return "Failed to write JSON file";
        case GameHistoryError::JsonSerializationFailed:
            return "Failed to serialize JSON data";
        case GameHistoryError::PermissionDenied:
            return "Permission denied";
        default:
            return "Unknown error";
    }
}

GameHistory::GameHistory(const cli_config_t& config) 
    : config_(config), move_count_(0) {
    
    game_start_time_ = std::chrono::system_clock::now();
    
    // Generate unique game ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    const char* hex_chars = "0123456789abcdef";
    
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) ss << '-';
        ss << hex_chars[dis(gen)];
    }
    
    game_id_ = ss.str();
}

std::string GameHistory::generate_filename() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto local_time = *std::localtime(&time_t);
    
    return std::format("gomoku-game.{:04d}{:02d}{:02d}.{:02d}{:02d}{:02d}.json",
                       local_time.tm_year + 1900,
                       local_time.tm_mon + 1,
                       local_time.tm_mday,
                       local_time.tm_hour,
                       local_time.tm_min,
                       local_time.tm_sec);
}

std::expected<void, GameHistoryError> GameHistory::ensure_directory_exists() {
    std::filesystem::path dir_path = "game_histories";
    
    try {
        if (!std::filesystem::exists(dir_path)) {
            if (!std::filesystem::create_directory(dir_path)) {
                return std::unexpected(GameHistoryError::DirectoryCreationFailed);
            }
        }
        return {};
    } catch (const std::filesystem::filesystem_error& e) {
        return std::unexpected(GameHistoryError::PermissionDenied);
    }
}

std::expected<void, GameHistoryError> GameHistory::initialize() {
    // Ensure directory exists
    auto dir_result = ensure_directory_exists();
    if (!dir_result) {
        fatal_error(std::format("Cannot create game_histories directory: {}", 
                               game_history_error_to_string(dir_result.error())));
        return std::unexpected(dir_result.error());
    }
    
    // Generate filename
    std::string filename = generate_filename();
    file_path_ = "game_histories/" + filename;
    
    // Initialize JSON structure
    game_json_ = json::object();
    
    // Schema version
    game_json_["version"] = "1.0";
    
    // Game metadata
    auto now_formatted = std::format("{:%Y-%m-%dT%H:%M:%SZ}", 
                                   std::chrono::floor<std::chrono::seconds>(game_start_time_));
    
    game_json_["game"] = {
        {"id", game_id_},
        {"status", "in_progress"},
        {"board_size", config_.board_size},
        {"created_at", now_formatted}
    };
    
    // Check if either player is computer-controlled
    bool has_ai = (std::string(config_.player1_type) == "computer" || 
                   std::string(config_.player2_type) == "computer");
    
    // AI configuration (if any computer players)
    if (has_ai) {
        game_json_["game"]["ai_config"] = {
            {"depth", config_.max_depth},
            {"timeout_ms", config_.move_timeout * 1000},
            {"threads", config_.thread_count > 0 ? config_.thread_count : 1}
        };
    }
    
    // Players configuration
    std::string player1_name = std::strlen(config_.player1_name) > 0 ? 
                              config_.player1_name : "Player 1";
    std::string player2_name = std::strlen(config_.player2_name) > 0 ? 
                              config_.player2_name : "Player 2";
    
    // Special handling for AI player names
    if (std::string(config_.player1_type) == "computer") {
        player1_name = "gomoku-cpp23";
    }
    if (std::string(config_.player2_type) == "computer") {
        player2_name = "gomoku-cpp23";
    }
    
    game_json_["players"] = {
        {"x", {
            {"nickname", player1_name}, 
            {"type", std::string(config_.player1_type) == "computer" ? "ai" : "human"}
        }},
        {"o", {
            {"nickname", player2_name}, 
            {"type", std::string(config_.player2_type) == "computer" ? "ai" : "human"}
        }}
    };
    
    // Initialize empty move history
    game_json_["moves"] = json::array();
    
    // Current player starts with X
    game_json_["current_player"] = "x";
    
    // Write initial file
    return write_json_file();
}

json GameHistory::serialize_move(const move_history_t& move, bool is_winning_move) {
    json move_json;
    
    // Determine player
    move_json["player"] = (move.player == static_cast<int>(Player::Cross)) ? "x" : "o";
    
    // Position
    move_json["position"] = {
        {"x", move.x},
        {"y", move.y}
    };
    
    // Timestamp
    auto now = std::chrono::system_clock::now();
    move_json["timestamp"] = std::format("{:%Y-%m-%dT%H:%M:%SZ}", 
                                       std::chrono::floor<std::chrono::seconds>(now));
    
    // Timing
    move_json["move_time_ms"] = static_cast<int>(move.time_taken * 1000);
    
    // AI-specific data
    if (move.positions_evaluated > 0) {
        move_json["positions_evaluated"] = move.positions_evaluated;
    }
    
    // Winning move status
    move_json["is_winning_move"] = is_winning_move;
    
    return move_json;
}

std::expected<void, GameHistoryError> GameHistory::log_move(
    const move_history_t& move, 
    bool is_winning_move) {
    
    try {
        // Add move to history
        json move_json = serialize_move(move, is_winning_move);
        game_json_["moves"].push_back(move_json);
        move_count_++;
        
        // Update current player for next turn
        std::string current_player = move_json["player"];
        game_json_["current_player"] = (current_player == "x") ? "o" : "x";
        
        // Write updated JSON to file
        return write_json_file();
        
    } catch (const json::exception& e) {
        fatal_error(std::format("JSON serialization failed: {}", e.what()));
        return std::unexpected(GameHistoryError::JsonSerializationFailed);
    }
}

std::expected<void, GameHistoryError> GameHistory::update_game_status(const std::string& status) {
    try {
        game_json_["game"]["status"] = status;
        return write_json_file();
    } catch (const json::exception& e) {
        fatal_error(std::format("Failed to update game status: {}", e.what()));
        return std::unexpected(GameHistoryError::JsonSerializationFailed);
    }
}

std::expected<void, GameHistoryError> GameHistory::write_json_file() {
    try {
        std::ofstream file(file_path_);
        if (!file.is_open()) {
            fatal_error(std::format("Cannot open file for writing: {}", file_path_));
            return std::unexpected(GameHistoryError::FileWriteFailed);
        }
        
        file << game_json_.dump(2) << std::endl;
        file.close();
        
        if (file.fail()) {
            fatal_error(std::format("Failed to write to file: {}", file_path_));
            return std::unexpected(GameHistoryError::FileWriteFailed);
        }
        
        return {};
        
    } catch (const std::exception& e) {
        fatal_error(std::format("File write error: {}", e.what()));
        return std::unexpected(GameHistoryError::FileWriteFailed);
    }
}

void GameHistory::fatal_error(const std::string& message) {
    std::cerr << COLOR_BRIGHT_RED << "FATAL ERROR: " << message << COLOR_RESET << std::endl;
    std::exit(1);
}

} // namespace gomoku