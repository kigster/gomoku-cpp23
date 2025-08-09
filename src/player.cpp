//
//  player.cpp
//  gomoku - Player implementation for different player types
//
//  Implementation of abstract Player hierarchy
//

#include "player.hpp"
#include "game.h"
#include "ui.hpp"
#include "ai.h"
#include "ai_parallel.hpp"
#include "game_coordinator.hpp"
#include <iostream>
#include <sstream>

namespace gomoku {

//===============================================================================
// HUMAN PLAYER IMPLEMENTATION
//===============================================================================

PlayerMoveResult HumanPlayer::make_move(GameStateWrapper* game_state) {
    double start_time = get_current_time();
    int x = -1, y = -1;
    bool move_made = false;

    // Handle input until a valid move is made
    handle_input(game_state, x, y, move_made);

    if (move_made && x >= 0 && y >= 0) {
        double time_taken = get_current_time() - start_time;
        return PlayerMoveResult::valid_move(x, y, time_taken, 1, "Human move");
    }

    return PlayerMoveResult::invalid();
}

void HumanPlayer::handle_input(GameStateWrapper* game_state, int& x, int& y, bool& move_made) {
    game_state_t* game = game_state->legacy_state;

    // This mirrors the existing input handling from ui.cpp
    int key = get_key();

    switch (key) {
        case 27: // ESC
            game->game_state = static_cast<int>(gomoku::GameState::Quit);
            break;

        case 72: // UP
        case 'w':
        case 'W':
            if (game->cursor_y > 0) {
                game->cursor_y--;
            } else {
                gomoku::ui::bad_beep(); // Cursor at top boundary
            }
            break;

        case 80: // DOWN
        case 's':
        case 'S':
            if (game->cursor_y < game->board_size - 1) {
                game->cursor_y++;
            } else {
                gomoku::ui::bad_beep(); // Cursor at bottom boundary
            }
            break;

        case 75: // LEFT
        case 'a':
        case 'A':
            if (game->cursor_x > 0) {
                game->cursor_x--;
            } else {
                gomoku::ui::bad_beep(); // Cursor at left boundary
            }
            break;

        case 77: // RIGHT
        case 'd':
        case 'D':
            if (game->cursor_x < game->board_size - 1) {
                game->cursor_x++;
            } else {
                gomoku::ui::bad_beep(); // Cursor at right boundary
            }
            break;

        case 13: // ENTER
        case 32: // SPACE
            if (is_valid_move(game->board, game->cursor_x, game->cursor_y, game->board_size)) {
                x = game->cursor_x;
                y = game->cursor_y;
                move_made = true;
            } else {
                gomoku::ui::bad_beep(); // Invalid move attempt
            }
            break;

        case 'u':
        case 'U':
            if (game->config.enable_undo && can_undo(game)) {
                undo_last_moves(game);
            } else {
                gomoku::ui::bad_beep(); // Undo not available or not enabled
            }
            break;

        case '+':
            // Increase AI difficulty 
            if (g_current_coordinator) {
                g_current_coordinator->increase_computer_difficulty();
            } else {
                gomoku::ui::bad_beep(); // No coordinator available
            }
            break;
            
        case '-':
            // Decrease AI difficulty
            if (g_current_coordinator) {
                g_current_coordinator->decrease_computer_difficulty(); 
            } else {
                gomoku::ui::bad_beep(); // No coordinator available
            }
            break;

        case '?':
            display_rules();
            break;
    }
}

//===============================================================================
// COMPUTER PLAYER IMPLEMENTATION
//===============================================================================

PlayerMoveResult ComputerPlayer::make_move(GameStateWrapper* game_state) {
    game_state_t* game = game_state->legacy_state;
    double start_time = get_current_time();

    int ai_x, ai_y;

    // Set the AI depth based on difficulty
    game->max_depth = static_cast<int>(difficulty_);

    // Use parallel AI if available, otherwise fall back to sequential
    if (g_parallel_ai) {
        find_best_ai_move_parallel(game, &ai_x, &ai_y);
    } else {
        find_best_ai_move(game, &ai_x, &ai_y);
    }
    double time_taken = get_current_time() - start_time;

    if (ai_x >= 0 && ai_y >= 0) {
        // Get positions evaluated from AI status message
        int positions_evaluated = 1;
        if (game->ai_history_count > 0) {
            std::sscanf(game->ai_history[game->ai_history_count - 1],
                       "%*d | %d positions evaluated", &positions_evaluated);
        }

        std::ostringstream desc;
        desc << name_ << " move (depth " << static_cast<int>(difficulty_) << ")";

        return PlayerMoveResult::valid_move(ai_x, ai_y, time_taken, positions_evaluated, desc.str());
    }

    return PlayerMoveResult::invalid();
}

void ComputerPlayer::on_game_start(GameStateWrapper* game_state) {
    game_state_t* game = game_state->legacy_state;

    // Set AI search depth based on difficulty
    game->max_depth = static_cast<int>(difficulty_);

    // Add AI status message
    std::ostringstream status;
    status << "Game On! [Level: ";
    switch (difficulty_) {
        case Difficulty::Easy: status << "Easy"; break;
        case Difficulty::Medium: status << "Medium"; break;
        case Difficulty::Hard: status << "Hard"; break;
    }
    status << " | Depth: " << static_cast<int>(difficulty_) << "]";

    std::strncpy(game->ai_status_message, status.str().c_str(),
                 sizeof(game->ai_status_message) - 1);
    game->ai_status_message[sizeof(game->ai_status_message) - 1] = '\0';
}

//===============================================================================
// PLAYER FACTORY IMPLEMENTATION
//===============================================================================

std::unique_ptr<PlayerImpl> PlayerFactory::create_player(PlayerSpec spec,
                                                    const std::string& name,
                                                    ComputerPlayer::Difficulty difficulty) {
    switch (spec) {
        case PlayerSpec::Human:
            return std::make_unique<HumanPlayer>(
                name.empty() ? "Human" : name
            );

        case PlayerSpec::Computer:
            return std::make_unique<ComputerPlayer>(
                name.empty() ? get_classical_name(difficulty) : name,
                difficulty
            );
    }

    return nullptr; // Should never reach here
}

std::pair<std::unique_ptr<PlayerImpl>, std::unique_ptr<PlayerImpl>>
PlayerFactory::create_player_pair(PlayerSpec player1, PlayerSpec player2) {
    auto p1 = create_player(player1, generate_default_name(player1, 1));
    auto p2 = create_player(player2, generate_default_name(player2, 2));

    return {std::move(p1), std::move(p2)};
}

std::string PlayerFactory::get_classical_name(ComputerPlayer::Difficulty difficulty) {
    switch (difficulty) {
        case ComputerPlayer::Difficulty::Easy:
            return "Plato";
        case ComputerPlayer::Difficulty::Medium:
            return "Socrates";
        case ComputerPlayer::Difficulty::Hard:
            return "Archimedes";
    }
    return "Computer"; // Fallback
}

std::string PlayerFactory::generate_default_name(PlayerSpec spec, int player_number) {
    switch (spec) {
        case PlayerSpec::Human:
            if (player_number == 1) return "Player 1";
            else return "Player 2";

        case PlayerSpec::Computer:
            if (player_number == 1) return "Computer 1";
            else return "Computer 2";
    }

    return "Unknown";
}

} // namespace gomoku
