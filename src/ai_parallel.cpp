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
// ENHANCED BRANCH-LEVEL PARALLELIZATION
//===============================================================================

int ParallelAI::parallel_minimax(game_state_t* game, int depth, int alpha, int beta,
                                bool maximizing_player, int ai_player, int last_x, int last_y,
                                int min_parallel_depth) {
    // Base cases - same as sequential minimax
    if (depth == 0 || game->search_timed_out) {
        return ::evaluate_position_incremental(game->board, game->board_size, ai_player, last_x, last_y);
    }
    
    // Check for immediate wins/losses
    if (::has_winner(game->board, ai_player, game->board_size)) {
        return maximizing_player ? WIN_SCORE - depth : LOSE_SCORE + depth;
    }
    
    int opponent = (ai_player == 1) ? -1 : 1;
    if (::has_winner(game->board, opponent, game->board_size)) {
        return maximizing_player ? LOSE_SCORE + depth : WIN_SCORE - depth;
    }
    
    // Generate moves for current position
    move_t moves[361];
    int current_player = maximizing_player ? ai_player : opponent;
    int move_count = generate_moves_optimized(game, moves, current_player);
    
    if (move_count == 0) {
        return 0; // Draw
    }
    
    // Sort moves by priority for better alpha-beta pruning
    qsort(moves, move_count, sizeof(move_t), compare_moves);
    
    // Decide whether to parallelize this level
    bool should_parallelize = (depth >= min_parallel_depth) && 
                              (move_count >= 4) && 
                              (thread_pool_.size() > 1) &&
                              !game->search_timed_out;
    
    if (should_parallelize && maximizing_player) {
        // PARALLEL BRANCH EVALUATION for maximizing player
        return parallel_max_node(game, moves, move_count, depth, alpha, beta, 
                                ai_player, min_parallel_depth);
    } else if (should_parallelize && !maximizing_player) {
        // PARALLEL BRANCH EVALUATION for minimizing player  
        return parallel_min_node(game, moves, move_count, depth, alpha, beta, 
                                ai_player, min_parallel_depth);
    } else {
        // SEQUENTIAL EVALUATION - fall back to regular minimax
        return sequential_minimax_node(game, moves, move_count, depth, alpha, beta,
                                     maximizing_player, ai_player, min_parallel_depth);
    }
}

int ParallelAI::parallel_max_node(game_state_t* game, move_t* moves, int move_count, 
                                 int depth, int alpha, int beta, int ai_player, int min_parallel_depth) {
    // Determine how many moves to evaluate in parallel
    int parallel_count = std::min(move_count, static_cast<int>(thread_pool_.size()));
    parallel_count = std::min(parallel_count, 6); // Cap for memory efficiency
    
    // Create tasks for parallel evaluation
    std::vector<ParallelMinimaxTask> tasks;
    std::vector<std::future<void>> futures;
    tasks.reserve(parallel_count);
    futures.reserve(parallel_count);
    
    std::atomic<int> shared_alpha{alpha};
    std::atomic<bool> cutoff_occurred{false};
    
    // Launch parallel tasks for the top moves
    for (int i = 0; i < parallel_count && !cutoff_occurred.load(); i++) {
        // Clone game state for thread safety
        game_state_t* game_clone = clone_game_state(game);
        if (!game_clone) continue;
        
        tasks.emplace_back();
        auto& task = tasks.back();
        task.game_clone = game_clone;
        task.move = moves[i];
        task.depth = depth - 1;
        task.alpha = shared_alpha.load();
        task.beta = beta;
        task.maximizing_player = false; // Next level is minimizing
        task.ai_player = ai_player;
        task.min_parallel_depth = min_parallel_depth;
        
        // Launch task
        futures.emplace_back(thread_pool_.enqueue([this, &task, &shared_alpha, &cutoff_occurred]() {
            try {
                // Apply move to cloned state
                task.game_clone->board[task.move.x][task.move.y] = task.ai_player;
                
                // Update hash for cloned state
                int player_index = (task.ai_player == 1) ? 1 : 0;
                int pos = task.move.x * task.game_clone->board_size + task.move.y;
                task.game_clone->current_hash ^= task.game_clone->zobrist_keys[player_index][pos];
                
                // Recursively evaluate this branch
                int score = parallel_minimax(task.game_clone, task.depth, task.alpha, task.beta,
                                           task.maximizing_player, task.ai_player, 
                                           task.move.x, task.move.y, task.min_parallel_depth);
                
                task.score->store(score);
                task.completed->store(true);
                
                // Update shared alpha if we found a better score
                int current_alpha = shared_alpha.load();
                while (score > current_alpha && 
                       !shared_alpha.compare_exchange_weak(current_alpha, score)) {
                    // Retry if concurrent update occurred
                }
                
                // Signal cutoff if beta cutoff condition met
                if (score >= task.beta) {
                    cutoff_occurred.store(true);
                }
                
            } catch (...) {
                task.completed->store(true); // Mark as completed even on error
            }
        }));
    }
    
    // Wait for parallel tasks to complete or cutoff
    int best_score = alpha;
    for (size_t i = 0; i < futures.size(); i++) {
        futures[i].wait();
        
        if (tasks[i].completed->load()) {
            int score = tasks[i].score->load();
            best_score = std::max(best_score, score);
            
            // Alpha-beta cutoff
            if (score >= beta) {
                // Clean up remaining tasks
                for (size_t j = i + 1; j < tasks.size(); j++) {
                    free_game_state_clone(tasks[j].game_clone);
                }
                
                // Clean up current tasks
                for (size_t j = 0; j <= i; j++) {
                    free_game_state_clone(tasks[j].game_clone);
                }
                
                return beta; // Cutoff
            }
            
            alpha = std::max(alpha, score);
        }
        
        free_game_state_clone(tasks[i].game_clone);
    }
    
    // Evaluate remaining moves sequentially with updated alpha
    for (int i = parallel_count; i < move_count && alpha < beta; i++) {
        if (game->search_timed_out) break;
        
        // Apply move
        game->board[moves[i].x][moves[i].y] = ai_player;
        
        // Update hash
        int player_index = (ai_player == 1) ? 1 : 0;
        int pos = moves[i].x * game->board_size + moves[i].y;
        game->current_hash ^= game->zobrist_keys[player_index][pos];
        
        int score = parallel_minimax(game, depth - 1, alpha, beta, false, ai_player, 
                                   moves[i].x, moves[i].y, min_parallel_depth);
        
        // Undo move
        game->current_hash ^= game->zobrist_keys[player_index][pos];
        game->board[moves[i].x][moves[i].y] = static_cast<int>(gomoku::Player::Empty);
        
        best_score = std::max(best_score, score);
        alpha = std::max(alpha, score);
        
        if (alpha >= beta) {
            break; // Beta cutoff
        }
    }
    
    return best_score;
}

int ParallelAI::parallel_min_node(game_state_t* game, move_t* moves, int move_count, 
                                 int depth, int alpha, int beta, int ai_player, int min_parallel_depth) {
    // Similar to max node but for minimizing player
    int parallel_count = std::min(move_count, static_cast<int>(thread_pool_.size()));
    parallel_count = std::min(parallel_count, 6);
    
    std::vector<ParallelMinimaxTask> tasks;
    std::vector<std::future<void>> futures;
    tasks.reserve(parallel_count);
    futures.reserve(parallel_count);
    
    std::atomic<int> shared_beta{beta};
    std::atomic<bool> cutoff_occurred{false};
    
    int opponent = (ai_player == 1) ? -1 : 1;
    
    // Launch parallel tasks
    for (int i = 0; i < parallel_count && !cutoff_occurred.load(); i++) {
        game_state_t* game_clone = clone_game_state(game);
        if (!game_clone) continue;
        
        tasks.emplace_back();
        auto& task = tasks.back();
        task.game_clone = game_clone;
        task.move = moves[i];
        task.depth = depth - 1;
        task.alpha = alpha;
        task.beta = shared_beta.load();
        task.maximizing_player = true; // Next level is maximizing
        task.ai_player = ai_player;
        task.min_parallel_depth = min_parallel_depth;
        
        futures.emplace_back(thread_pool_.enqueue([this, &task, &shared_beta, &cutoff_occurred, opponent]() {
            try {
                // Apply opponent move
                task.game_clone->board[task.move.x][task.move.y] = opponent;
                
                // Update hash
                int player_index = (opponent == 1) ? 1 : 0;
                int pos = task.move.x * task.game_clone->board_size + task.move.y;
                task.game_clone->current_hash ^= task.game_clone->zobrist_keys[player_index][pos];
                
                int score = parallel_minimax(task.game_clone, task.depth, task.alpha, task.beta,
                                           task.maximizing_player, task.ai_player, 
                                           task.move.x, task.move.y, task.min_parallel_depth);
                
                task.score->store(score);
                task.completed->store(true);
                
                // Update shared beta if we found a worse score (for minimizing)
                int current_beta = shared_beta.load();
                while (score < current_beta && 
                       !shared_beta.compare_exchange_weak(current_beta, score)) {
                    // Retry if concurrent update occurred
                }
                
                if (score <= task.alpha) {
                    cutoff_occurred.store(true);
                }
                
            } catch (...) {
                task.completed->store(true);
            }
        }));
    }
    
    // Wait and collect results
    int best_score = beta;
    for (size_t i = 0; i < futures.size(); i++) {
        futures[i].wait();
        
        if (tasks[i].completed->load()) {
            int score = tasks[i].score->load();
            best_score = std::min(best_score, score);
            
            if (score <= alpha) {
                // Clean up and return cutoff
                for (size_t j = i; j < tasks.size(); j++) {
                    free_game_state_clone(tasks[j].game_clone);
                }
                for (size_t j = 0; j < i; j++) {
                    free_game_state_clone(tasks[j].game_clone);
                }
                return alpha;
            }
            
            beta = std::min(beta, score);
        }
        
        free_game_state_clone(tasks[i].game_clone);
    }
    
    // Evaluate remaining moves sequentially
    for (int i = parallel_count; i < move_count && alpha < beta; i++) {
        if (game->search_timed_out) break;
        
        game->board[moves[i].x][moves[i].y] = opponent;
        
        int player_index = (opponent == 1) ? 1 : 0;
        int pos = moves[i].x * game->board_size + moves[i].y;
        game->current_hash ^= game->zobrist_keys[player_index][pos];
        
        int score = parallel_minimax(game, depth - 1, alpha, beta, true, ai_player, 
                                   moves[i].x, moves[i].y, min_parallel_depth);
        
        game->current_hash ^= game->zobrist_keys[player_index][pos];
        game->board[moves[i].x][moves[i].y] = static_cast<int>(gomoku::Player::Empty);
        
        best_score = std::min(best_score, score);
        beta = std::min(beta, score);
        
        if (beta <= alpha) {
            break; // Alpha cutoff
        }
    }
    
    return best_score;
}

int ParallelAI::sequential_minimax_node(game_state_t* game, move_t* moves, int move_count, 
                                       int depth, int alpha, int beta, bool maximizing_player, 
                                       int ai_player, int min_parallel_depth) {
    // Fall back to regular minimax when parallelization isn't beneficial
    if (maximizing_player) {
        int best_score = alpha;
        
        for (int i = 0; i < move_count && alpha < beta; i++) {
            if (game->search_timed_out) break;
            
            game->board[moves[i].x][moves[i].y] = ai_player;
            
            int player_index = (ai_player == 1) ? 1 : 0;
            int pos = moves[i].x * game->board_size + moves[i].y;
            game->current_hash ^= game->zobrist_keys[player_index][pos];
            
            int score = parallel_minimax(game, depth - 1, alpha, beta, false, ai_player, 
                                       moves[i].x, moves[i].y, min_parallel_depth);
            
            game->current_hash ^= game->zobrist_keys[player_index][pos];
            game->board[moves[i].x][moves[i].y] = static_cast<int>(gomoku::Player::Empty);
            
            best_score = std::max(best_score, score);
            alpha = std::max(alpha, score);
        }
        
        return best_score;
    } else {
        int best_score = beta;
        int opponent = (ai_player == 1) ? -1 : 1;
        
        for (int i = 0; i < move_count && alpha < beta; i++) {
            if (game->search_timed_out) break;
            
            game->board[moves[i].x][moves[i].y] = opponent;
            
            int player_index = (opponent == 1) ? 1 : 0;
            int pos = moves[i].x * game->board_size + moves[i].y;
            game->current_hash ^= game->zobrist_keys[player_index][pos];
            
            int score = parallel_minimax(game, depth - 1, alpha, beta, true, ai_player, 
                                       moves[i].x, moves[i].y, min_parallel_depth);
            
            game->current_hash ^= game->zobrist_keys[player_index][pos];
            game->board[moves[i].x][moves[i].y] = static_cast<int>(gomoku::Player::Empty);
            
            best_score = std::min(best_score, score);
            beta = std::min(beta, score);
        }
        
        return best_score;
    }
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