//
//  game_history.hpp
//  gomoku - Game history JSON logging functionality
//
//  Modern C++23 JSON logging compatible with HTTP daemon schema
//

#pragma once

#include <string>
#include <filesystem>
#include <chrono>
#include <expected>
#include <format>

#include "json.hpp"
#include "game.h"
#include "cli.hpp"

namespace gomoku {

using json = nlohmann::json;

enum class GameHistoryError {
    DirectoryCreationFailed,
    FileWriteFailed,
    JsonSerializationFailed,
    PermissionDenied
};

class GameHistory {
public:
    explicit GameHistory(const cli_config_t& config);
    ~GameHistory() = default;

    // Initialize the game history (creates directory and initial file)
    std::expected<void, GameHistoryError> initialize();
    
    // Log a move to the history file
    std::expected<void, GameHistoryError> log_move(
        const move_history_t& move, 
        bool is_winning_move = false
    );
    
    // Update game status
    std::expected<void, GameHistoryError> update_game_status(const std::string& status);
    
    // Get the current JSON file path
    std::string get_file_path() const { return file_path_; }
    
private:
    // Generate timestamped filename
    std::string generate_filename();
    
    // Create game_histories directory if it doesn't exist
    std::expected<void, GameHistoryError> ensure_directory_exists();
    
    // Write JSON to file with error handling
    std::expected<void, GameHistoryError> write_json_file();
    
    // Convert move_history_t to JSON format
    json serialize_move(const move_history_t& move, bool is_winning_move = false);
    
    // Print fatal error and exit
    void fatal_error(const std::string& message);
    
    cli_config_t config_;
    json game_json_;
    std::string file_path_;
    std::string game_id_;
    std::chrono::system_clock::time_point game_start_time_;
    int move_count_;
};

const char* game_history_error_to_string(GameHistoryError error);

} // namespace gomoku