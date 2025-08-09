//
//  ai_parallel.cpp
//  gomoku - Parallel AI search implementation
//
//  Implements parallel root-level search with thread pool
//

#include "ai_parallel.hpp"
#include "gomoku.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

namespace gomoku {

// Global parallel AI instance
std::unique_ptr<ParallelAI> g_parallel_ai = nullptr;

//===============================================================================
// PARALLEL AI IMPLEMENTATION
//===============================================================================

ParallelAI::ParallelAI(size_t num_threads) 
    : thread_pool_(num_threads == 0 ? 
        std::max(1u, std::thread::hardware_concurrency() - 1) : num_threads) {
    
    // No need for fallback since we already handle 0 case above
}

void ParallelAI::find_best_move_parallel(game_state_t* game, int* best_x, int* best_y) {
    // Fallback to sequential for very early game or timeout situations
    if (game->stones_on_board < 2 || game->move_timeout > 0) {
        find_best_ai_move(game, best_x, best_y);
        return;
    }
    
    // Generate moves using existing optimized system
    move_t moves[361]; // Max for 19x19 board
    int current_player = static_cast<int>(gomoku::Player::Naught); // AI player
    int move_count = generate_moves_optimized(game, moves, current_player);
    
    if (move_count == 0) {
        *best_x = -1;
        *best_y = -1;
        return;
    }
    
    if (move_count == 1) {
        *best_x = moves[0].x;
        *best_y = moves[0].y;
        return;
    }
    
    // Sort moves by priority (best first) - helps with parallelization
    qsort(moves, move_count, sizeof(move_t), compare_moves);
    
    // Limit concurrent evaluations to prevent too much overhead
    int max_parallel = std::min(move_count, static_cast<int>(thread_pool_.size()));
    max_parallel = std::min(max_parallel, 8); // Cap at 8 for memory reasons
    
    // Initialize parallel search state
    ParallelSearchState search_state;
    
    // Create move evaluations
    std::vector<MoveEvaluation> move_evals;
    move_evals.reserve(move_count);
    
    for (int i = 0; i < move_count; i++) {
        move_evals.emplace_back(moves[i].x, moves[i].y, moves[i].priority);
    }
    
    // Launch parallel evaluations for top moves
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < max_parallel; i++) {
        futures.emplace_back(
            thread_pool_.enqueue([this, game, &move_evals, &search_state, i]() {
                this->evaluate_move_parallel(game, &move_evals[i], &search_state);
            })
        );
    }
    
    // Wait for parallel evaluations to complete
    for (auto& future : futures) {
        future.wait();
    }
    
    // Evaluate remaining moves sequentially if any
    for (int i = max_parallel; i < move_count; i++) {
        if (search_state.timeout_occurred.load()) {
            break;
        }
        evaluate_move_parallel(game, &move_evals[i], &search_state);
    }
    
    // Find best move from completed evaluations
    int best_score = -WIN_SCORE - 1;
    *best_x = -1;
    *best_y = -1;
    
    for (const auto& eval : move_evals) {
        if (eval.completed && eval.score > best_score) {
            best_score = eval.score;
            *best_x = eval.x;
            *best_y = eval.y;
        }
    }
    
    // Update AI history with parallel search info
    if (game->ai_history_count < MAX_AI_HISTORY) {
        snprintf(game->ai_history[game->ai_history_count], 
                sizeof(game->ai_history[game->ai_history_count]),
                "%d | %d positions evaluated (%zu threads)",
                game->ai_history_count + 1, 
                search_state.moves_evaluated.load(),
                thread_pool_.size());
        game->ai_history_count++;
    }
}

void ParallelAI::evaluate_move_parallel(game_state_t* game, MoveEvaluation* move_eval, 
                                        ParallelSearchState* search_state) {
    
    if (search_state->timeout_occurred.load()) {
        return;
    }
    
    // Clone game state for thread safety
    game_state_t* game_clone = clone_game_state(game);
    if (!game_clone) {
        return;
    }
    
    try {
        // Make the move on cloned state
        int x = move_eval->x;
        int y = move_eval->y;
        int ai_player = static_cast<int>(gomoku::Player::Naught);
        
        game_clone->board[x][y] = ai_player;
        
        // Update hash incrementally for cloned state
        int player_index = 1; // Naught player
        int pos = x * game_clone->board_size + y;
        game_clone->current_hash ^= game_clone->zobrist_keys[player_index][pos];
        
        // Get current alpha-beta bounds
        int alpha = search_state->global_alpha.load();
        int beta = search_state->global_beta.load();
        
        // Evaluate using sequential minimax on cloned state
        int score = minimax_with_timeout(game_clone, game_clone->board, 
                                        game_clone->max_depth - 1, alpha, beta, 
                                        0, ai_player, x, y);
        
        // Update global bounds if we found a better move
        if (score > search_state->best_score.load()) {
            std::lock_guard<std::mutex> lock(search_state->best_move_mutex);
            if (score > search_state->best_score.load()) {
                search_state->best_score.store(score);
                search_state->global_alpha.store(std::max(alpha, score));
            }
        }
        
        // Store result
        move_eval->score = score;
        move_eval->completed = true;
        
        search_state->moves_evaluated.fetch_add(1);
        
    } catch (...) {
        // Error in evaluation - mark as incomplete
        move_eval->completed = false;
    }
    
    free_game_state_clone(game_clone);
}

game_state_t* ParallelAI::clone_game_state(game_state_t* original) {
    game_state_t* clone = new(std::nothrow) game_state_t;
    if (!clone) return nullptr;
    
    // Copy the structure
    *clone = *original;
    
    // Clone the board
    clone->board = create_board(original->board_size);
    if (!clone->board) {
        delete clone;
        return nullptr;
    }
    
    // Copy board data
    for (int i = 0; i < original->board_size; i++) {
        for (int j = 0; j < original->board_size; j++) {
            clone->board[i][j] = original->board[i][j];
        }
    }
    
    // Copy other necessary data
    memcpy(clone->zobrist_keys, original->zobrist_keys, sizeof(original->zobrist_keys));
    memcpy(clone->interesting_moves, original->interesting_moves, sizeof(original->interesting_moves));
    
    return clone;
}

void ParallelAI::free_game_state_clone(game_state_t* clone) {
    if (!clone) return;
    
    if (clone->board) {
        free_board(clone->board, clone->board_size);
    }
    
    delete clone;
}

//===============================================================================
// GLOBAL FUNCTIONS
//===============================================================================

void init_parallel_ai(size_t num_threads) {
    g_parallel_ai = std::make_unique<ParallelAI>(num_threads);
}

void cleanup_parallel_ai() {
    g_parallel_ai.reset();
}

} // namespace gomoku

//===============================================================================
// C-COMPATIBLE INTERFACE
//===============================================================================

extern "C" {

void find_best_ai_move_parallel(game_state_t* game, int* best_x, int* best_y) {
    if (gomoku::g_parallel_ai) {
        gomoku::g_parallel_ai->find_best_move_parallel(game, best_x, best_y);
    } else {
        // Fallback to sequential
        find_best_ai_move(game, best_x, best_y);
    }
}

}