//
//  game_state_wrapper.hpp
//  gomoku - Temporary bridge between legacy C game state and modern C++ players
//
//  This wrapper will be removed when game state is fully modernized
//

#pragma once

#include "game.h"

namespace gomoku {

/**
 * Temporary bridge wrapper for legacy game state
 * This allows modern Player classes to interact with legacy game_state_t
 */
struct GameStateWrapper {
    game_state_t* legacy_state;
    
    explicit GameStateWrapper(game_state_t* state) : legacy_state(state) {}
    
    // Convenience accessors (if needed by players)
    int get_current_player() const { return legacy_state->current_player; }
    int** get_board() const { return legacy_state->board; }
    int get_board_size() const { return legacy_state->board_size; }
    int get_cursor_x() const { return legacy_state->cursor_x; }
    int get_cursor_y() const { return legacy_state->cursor_y; }
    int get_game_state() const { return legacy_state->game_state; }
    
    void set_cursor(int x, int y) { 
        legacy_state->cursor_x = x; 
        legacy_state->cursor_y = y; 
    }
    
    void set_current_player(int player) { 
        legacy_state->current_player = player; 
    }
};

} // namespace gomoku