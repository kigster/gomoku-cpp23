//
//  game_coordinator.hpp  
//  gomoku - Game coordination using modern Player hierarchy
//
//  Coordinates game flow with abstract Player types
//

#pragma once

#include "player.hpp"
#include "game.h"
#include "cli.hpp"
#include <memory>
#include <string>

// Forward declaration for global coordinator access
namespace gomoku {
    class GameCoordinator;
    extern GameCoordinator* g_current_coordinator; // Global pointer for input handling
}

namespace gomoku {

/**
 * Modern game coordinator that manages different player types
 */
class GameCoordinator {
public:
    /**
     * Constructor - creates game coordinator with player configuration
     */
    explicit GameCoordinator(const cli_config_t& config);
    
    /**
     * Destructor
     */
    ~GameCoordinator();
    
    /**
     * Run the main game loop
     * @return Exit code (0 = success)
     */
    int run_game();
    
    /**
     * Get current game state (for UI access)
     */
    game_state_t* get_game_state() { return game_state_; }
    
    /**
     * Get current player
     */
    PlayerImpl* get_current_player() { 
        return current_player_index_ == 0 ? player1_.get() : player2_.get(); 
    }
    
    /**
     * Get player names for display
     */
    std::string get_player1_name() const { return player1_->get_name(); }
    std::string get_player2_name() const { return player2_->get_name(); }
    
    /**
     * Get player types for display
     */
    PlayerType get_player1_type() const { return player1_->get_type(); }
    PlayerType get_player2_type() const { return player2_->get_type(); }
    
    /**
     * Adjust difficulty of computer players
     */
    void increase_computer_difficulty();
    void decrease_computer_difficulty();
    
private:
    // Game state (legacy C struct)
    game_state_t* game_state_;
    
    // Modern Player objects
    std::unique_ptr<PlayerImpl> player1_;  // Moves first (crosses)
    std::unique_ptr<PlayerImpl> player2_;  // Moves second (naughts)
    
    // Current player tracking
    int current_player_index_;  // 0 = player1, 1 = player2
    
    /**
     * Initialize players from configuration
     */
    void initialize_players(const cli_config_t& config);
    
    /**
     * Make a move for the current player
     */
    bool make_player_move();
    
    /**
     * Switch to the next player
     */
    void switch_player();
    
    /**
     * Handle human vs human games (ask for names if needed)
     */
    void handle_human_vs_human_setup();
    
    /**
     * Get player symbol for display
     */
    gomoku::Player get_player_symbol(int player_index) const;
    
    /**
     * Convert player configuration to factory spec
     */
    PlayerFactory::PlayerSpec config_to_spec(const std::string& type) const;
    
    /**
     * Convert difficulty string to enum
     */
    ComputerPlayer::Difficulty string_to_difficulty(const std::string& diff) const;
};

} // namespace gomoku