//
//  game_coordinator.cpp
//  gomoku - Game coordination implementation
//
//  Manages game flow with Player hierarchy
//

#include "game_coordinator.hpp"
#include "ai_parallel.hpp"
#include "ui.hpp"
#include "gomoku.hpp"
#include <iostream>
#include <format>

namespace gomoku {

// Global pointer to current coordinator for input handling
GameCoordinator* g_current_coordinator = nullptr;

//===============================================================================
// CONSTRUCTOR / DESTRUCTOR
//===============================================================================

GameCoordinator::GameCoordinator(const cli_config_t& config) 
    : game_state_(nullptr), current_player_index_(0) {
    
    // Set global coordinator pointer
    g_current_coordinator = this;
    
    // Initialize legacy game state
    cli_config_t legacy_config = config;  // Make a copy
    game_state_ = init_game(legacy_config);
    
    if (!game_state_) {
        throw std::runtime_error("Failed to initialize game state");
    }
    
    // Initialize parallel AI with specified thread count
    size_t thread_count = config.thread_count > 0 ? 
        static_cast<size_t>(config.thread_count) : 0;
    init_parallel_ai(thread_count);
    
    // Initialize players from configuration
    initialize_players(config);
    
    // Initialize threat matrix
    populate_threat_matrix();
    
    // Notify players that game is starting
    GameStateWrapper bridge(game_state_);
    if (player1_) player1_->on_game_start(&bridge);
    if (player2_) player2_->on_game_start(&bridge);
}

GameCoordinator::~GameCoordinator() {
    // Notify players that game is ending
    if (game_state_) {
        GameStateWrapper bridge(game_state_);
        if (player1_) player1_->on_game_end(&bridge);
        if (player2_) player2_->on_game_end(&bridge);
    }
    
    // Cleanup resources
    if (game_state_) {
        cleanup_game(game_state_);
    }
    
    cleanup_parallel_ai();
    
    // Clear global coordinator pointer
    g_current_coordinator = nullptr;
}

//===============================================================================
// GAME LOOP
//===============================================================================

int GameCoordinator::run_game() {
    // Enable raw mode for input
    enable_raw_mode();
    
    // Handle special setup for human vs human
    if (player1_->get_type() == PlayerType::Human && 
        player2_->get_type() == PlayerType::Human) {
        handle_human_vs_human_setup();
    }
    
    // Main game loop
    while (game_state_->game_state == static_cast<int>(GameState::Running)) {
        // Refresh enhanced display with player information
        PlayerImpl* current_player = get_current_player();
        std::string current_name = current_player ? current_player->get_name() : "Unknown";
        
        gomoku::ui::refresh_display_with_players(game_state_, 
                                               get_player1_name(),
                                               get_player2_name(),
                                               current_name);
        
        // Make move for current player
        bool move_made = make_player_move();
        
        if (move_made) {
            // Check game state for win/draw
            check_game_state(game_state_);
            
            // Switch to next player
            switch_player();
        }
    }
    
    // Game ended - show final state
    if (game_state_->game_state != static_cast<int>(GameState::Quit)) {
        PlayerImpl* current_player = get_current_player();
        std::string current_name = current_player ? current_player->get_name() : "Unknown";
        
        gomoku::ui::refresh_display_with_players(game_state_, 
                                               get_player1_name(),
                                               get_player2_name(),
                                               current_name);
        get_key(); // Wait for any key press
    }
    
    return 0;
}

//===============================================================================
// PLAYER MANAGEMENT
//===============================================================================

void GameCoordinator::initialize_players(const cli_config_t& config) {
    // Create player 1 (moves first)
    auto spec1 = config_to_spec(config.player1_type);
    std::string name1 = config.player1_name[0] ? config.player1_name : "";
    auto difficulty1 = string_to_difficulty(config.player1_difficulty);
    
    player1_ = PlayerFactory::create_player(spec1, name1, difficulty1);
    
    // Create player 2 (moves second)  
    auto spec2 = config_to_spec(config.player2_type);
    std::string name2 = config.player2_name[0] ? config.player2_name : "";
    auto difficulty2 = string_to_difficulty(config.player2_difficulty);
    
    player2_ = PlayerFactory::create_player(spec2, name2, difficulty2);
    
    // Set initial player (player1 always goes first with crosses)
    current_player_index_ = 0;
    game_state_->current_player = static_cast<int>(Player::Cross);
}

bool GameCoordinator::make_player_move() {
    PlayerImpl* current_player = get_current_player();
    if (!current_player) return false;
    
    start_move_timer(game_state_);
    
    // Create GameStateWrapper bridge for player
    GameStateWrapper bridge(game_state_);
    
    // Get move from player
    PlayerMoveResult result = current_player->make_move(&bridge);
    
    double elapsed_time = end_move_timer(game_state_);
    
    if (result.valid) {
        // Make the move
        gomoku::Player symbol = get_player_symbol(current_player_index_);
        bool success = make_move(game_state_, result.x, result.y, 
                               static_cast<int>(symbol), 
                               elapsed_time, result.positions_evaluated);
        
        if (success) {
            // Track last AI move for highlighting
            if (current_player->get_type() == PlayerType::Computer) {
                game_state_->last_ai_move_x = result.x;
                game_state_->last_ai_move_y = result.y;
            }
            
            // Notify opponent of the move
            PlayerImpl* opponent = current_player_index_ == 0 ? player2_.get() : player1_.get();
            if (opponent) {
                opponent->on_opponent_move(&bridge, result.x, result.y);
            }
            
            return true;
        }
    }
    
    return false;
}

void GameCoordinator::switch_player() {
    current_player_index_ = 1 - current_player_index_;
    
    // Update legacy game state
    if (current_player_index_ == 0) {
        game_state_->current_player = static_cast<int>(Player::Cross);
    } else {
        game_state_->current_player = static_cast<int>(Player::Naught);
    }
}

//===============================================================================
// HELPER FUNCTIONS
//===============================================================================

void GameCoordinator::handle_human_vs_human_setup() {
    // If both players are human and don't have names, ask for them
    if (player1_->get_name() == "Human" && player2_->get_name() == "Human") {
        std::cout << "\nTwo human players detected. Please enter names:\n";
        
        std::string name1, name2;
        std::cout << "Player 1 (X) name: ";
        std::getline(std::cin, name1);
        if (!name1.empty()) {
            player1_ = std::make_unique<HumanPlayer>(name1);
        }
        
        std::cout << "Player 2 (O) name: ";
        std::getline(std::cin, name2);
        if (!name2.empty()) {
            player2_ = std::make_unique<HumanPlayer>(name2);
        }
        
        std::cout << "\nGame starting: " << player1_->get_name() 
                  << " (X) vs " << player2_->get_name() << " (O)\n";
        std::cout << "Press any key to continue...\n";
        get_key();
    }
}

gomoku::Player GameCoordinator::get_player_symbol(int player_index) const {
    return player_index == 0 ? gomoku::Player::Cross : gomoku::Player::Naught;
}

PlayerFactory::PlayerSpec GameCoordinator::config_to_spec(const std::string& type) const {
    if (type == "human") {
        return PlayerFactory::PlayerSpec::Human;
    } else if (type == "computer") {
        return PlayerFactory::PlayerSpec::Computer;
    }
    
    // Default fallback
    return PlayerFactory::PlayerSpec::Human;
}

ComputerPlayer::Difficulty GameCoordinator::string_to_difficulty(const std::string& diff) const {
    if (diff == "easy") {
        return ComputerPlayer::Difficulty::Easy;
    } else if (diff == "hard") {
        return ComputerPlayer::Difficulty::Hard;
    }
    
    // Default to medium
    return ComputerPlayer::Difficulty::Medium;
}

void GameCoordinator::increase_computer_difficulty() {
    // Find computer players and increase their difficulty
    bool changed = false;
    
    if (player1_->get_type() == PlayerType::Computer) {
        ComputerPlayer* comp = static_cast<ComputerPlayer*>(player1_.get());
        auto current = comp->get_difficulty();
        if (current == ComputerPlayer::Difficulty::Easy) {
            comp->set_difficulty(ComputerPlayer::Difficulty::Medium);
            comp->set_name("Socrates"); // Update to medium name
            changed = true;
        } else if (current == ComputerPlayer::Difficulty::Medium) {
            comp->set_difficulty(ComputerPlayer::Difficulty::Hard);
            comp->set_name("Archimedes"); // Update to hard name
            changed = true;
        } else {
            gomoku::ui::bad_beep(); // Already at maximum difficulty
        }
    }
    
    if (player2_->get_type() == PlayerType::Computer) {
        ComputerPlayer* comp = static_cast<ComputerPlayer*>(player2_.get());
        auto current = comp->get_difficulty();
        if (current == ComputerPlayer::Difficulty::Easy) {
            comp->set_difficulty(ComputerPlayer::Difficulty::Medium);
            comp->set_name("Socrates"); // Update to medium name
            changed = true;
        } else if (current == ComputerPlayer::Difficulty::Medium) {
            comp->set_difficulty(ComputerPlayer::Difficulty::Hard);
            comp->set_name("Archimedes"); // Update to hard name
            changed = true;
        } else {
            gomoku::ui::bad_beep(); // Already at maximum difficulty
        }
    }
    
    if (!changed && (player1_->get_type() != PlayerType::Computer && player2_->get_type() != PlayerType::Computer)) {
        gomoku::ui::bad_beep(); // No computer players to adjust
    }
}

void GameCoordinator::decrease_computer_difficulty() {
    // Find computer players and decrease their difficulty
    bool changed = false;
    
    if (player1_->get_type() == PlayerType::Computer) {
        ComputerPlayer* comp = static_cast<ComputerPlayer*>(player1_.get());
        auto current = comp->get_difficulty();
        if (current == ComputerPlayer::Difficulty::Hard) {
            comp->set_difficulty(ComputerPlayer::Difficulty::Medium);
            comp->set_name("Socrates"); // Update to medium name
            changed = true;
        } else if (current == ComputerPlayer::Difficulty::Medium) {
            comp->set_difficulty(ComputerPlayer::Difficulty::Easy);
            comp->set_name("Plato"); // Update to easy name
            changed = true;
        } else {
            gomoku::ui::bad_beep(); // Already at minimum difficulty
        }
    }
    
    if (player2_->get_type() == PlayerType::Computer) {
        ComputerPlayer* comp = static_cast<ComputerPlayer*>(player2_.get());
        auto current = comp->get_difficulty();
        if (current == ComputerPlayer::Difficulty::Hard) {
            comp->set_difficulty(ComputerPlayer::Difficulty::Medium);
            comp->set_name("Socrates"); // Update to medium name
            changed = true;
        } else if (current == ComputerPlayer::Difficulty::Medium) {
            comp->set_difficulty(ComputerPlayer::Difficulty::Easy);
            comp->set_name("Plato"); // Update to easy name
            changed = true;
        } else {
            gomoku::ui::bad_beep(); // Already at minimum difficulty
        }
    }
    
    if (!changed && (player1_->get_type() != PlayerType::Computer && player2_->get_type() != PlayerType::Computer)) {
        gomoku::ui::bad_beep(); // No computer players to adjust
    }
}

} // namespace gomoku