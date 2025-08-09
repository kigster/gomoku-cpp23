//
//  cli.hpp
//  gomoku - Modern C++23 Command Line Interface module
//
//  Handles command-line argument parsing and help display using modern C++
//

#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <span>
#include <optional>
#include "gomoku.hpp"

namespace gomoku::cli {

//===============================================================================
// CLI CONFIGURATION STRUCTURE
//===============================================================================

/**
 * Player configuration pair
 */
struct PlayerConfig {
    std::string type;      // "human" or "computer"
    std::string name;      // Optional name for the player
    std::string difficulty; // For computer players: "easy", "medium", "hard"
    
    PlayerConfig() = default;
    PlayerConfig(std::string_view t) : type(t) {}
    PlayerConfig(std::string_view t, std::string_view n) : type(t), name(n) {}
    PlayerConfig(std::string_view t, std::string_view n, std::string_view d) 
        : type(t), name(n), difficulty(d) {}
};

/**
 * Modern C++23 configuration structure with strong typing
 */
struct Config {
    int board_size = 19;         // Board size (15 or 19)
    int max_depth = 4;           // AI search depth (default intermediate)
    int move_timeout = 0;        // Move timeout in seconds (0 = no timeout)
    int thread_count = 0;        // Number of threads for parallel AI (0 = auto)
    bool show_help = false;      // Whether to show help and exit
    bool enable_undo = false;    // Whether to enable undo feature
    bool skip_welcome = false;   // Whether to skip the welcome screen
    
    // New player configuration
    PlayerConfig player1 = PlayerConfig("human");    // First player (moves first)
    PlayerConfig player2 = PlayerConfig("computer"); // Second player

    // Validation method
    [[nodiscard]] std::expected<void, std::string> validate() const;
    
    // Convert to legacy C struct for interoperability
    [[nodiscard]] auto to_c_struct() const;
};

//===============================================================================
// ERROR TYPES
//===============================================================================

enum class ParseError {
    InvalidArgument,
    InvalidBoardSize,
    InvalidDepth,
    InvalidTimeout,
    UnknownOption,
    MissingValue
};

//===============================================================================
// CLI FUNCTIONS
//===============================================================================

/**
 * Parses command line arguments using modern C++23 features.
 * 
 * @param args Span of command line arguments
 * @return Expected configuration or error
 */
[[nodiscard]] std::expected<Config, ParseError> 
parse_arguments_modern(std::span<const char*> args);

/**
 * Prints help message with usage information.
 * 
 * @param program_name The name of the program
 */
void print_help_modern(std::string_view program_name);

/**
 * Converts ParseError to human-readable string.
 * 
 * @param error The error to convert
 * @return Error message string
 */
[[nodiscard]] std::string_view error_to_string(ParseError error);

} // namespace gomoku::cli

//===============================================================================
// C-COMPATIBLE INTERFACE (for gradual migration)
//===============================================================================

// Legacy C structure for compatibility (must be defined outside extern "C" in C++)
typedef struct {
    int board_size;
    int max_depth;
    int move_timeout;
    int thread_count;             // Number of threads for parallel AI (0 = auto)
    int show_help;
    int invalid_args;
    int enable_undo;
    int skip_welcome;
    
    // Player configurations (extended for new functionality)
    char player1_type[16];        // "human" or "computer"
    char player1_name[64];        // Player 1 name
    char player1_difficulty[16];  // For computer: "easy", "medium", "hard"
    
    char player2_type[16];        // "human" or "computer" 
    char player2_name[64];        // Player 2 name
    char player2_difficulty[16];  // For computer: "easy", "medium", "hard"
} cli_config_t;

extern "C" {

    // C-compatible function declarations
    cli_config_t parse_arguments(int argc, char* argv[]);
    void print_help(const char* program_name);
    int validate_config(const cli_config_t* config);
}