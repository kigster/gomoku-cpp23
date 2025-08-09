//
//  ai_parallel.hpp
//  gomoku - Parallel AI search using thread pool for better performance
//
//  Implements parallel root-level search with alpha-beta pruning
//

#pragma once

#include "ai.h"
#include "util/thread_pool.hpp"
#include <atomic>
#include <mutex>
#include <vector>
#include <memory>

namespace gomoku {

/**
 * Parallel AI search engine using thread pool for root-level parallelization
 */
class ParallelAI {
public:
    /**
     * Constructor - initializes thread pool
     * @param num_threads Number of threads to use (0 = auto-detect)
     */
    explicit ParallelAI(size_t num_threads = 0);
    
    /**
     * Find best move using parallel search at root level
     * @param game Game state
     * @param best_x Output: best x coordinate
     * @param best_y Output: best y coordinate
     */
    void find_best_move_parallel(game_state_t* game, int* best_x, int* best_y);
    
    /**
     * Get the number of threads being used
     */
    size_t get_thread_count() const { return thread_pool_.size(); }
    
private:
    /**
     * Structure to hold move evaluation results
     */
    struct MoveEvaluation {
        int x, y;
        int score;
        int priority;
        bool completed;
        
        MoveEvaluation() : x(-1), y(-1), score(-WIN_SCORE-1), priority(0), completed(false) {}
        MoveEvaluation(int x_, int y_, int priority_) 
            : x(x_), y(y_), score(-WIN_SCORE-1), priority(priority_), completed(false) {}
    };
    
    /**
     * Parallel root search state
     */
    struct ParallelSearchState {
        std::atomic<int> global_alpha{-WIN_SCORE-1};
        std::atomic<int> global_beta{WIN_SCORE+1};
        std::atomic<bool> timeout_occurred{false};
        std::atomic<int> best_score{-WIN_SCORE-1};
        std::atomic<int> moves_evaluated{0};
        
        std::mutex best_move_mutex;
        int best_x{-1};
        int best_y{-1};
    };
    
    /**
     * Evaluate a single move in parallel
     */
    void evaluate_move_parallel(game_state_t* game, MoveEvaluation* move_eval, 
                               ParallelSearchState* search_state);
    
    /**
     * Create a deep copy of game state for thread safety
     */
    game_state_t* clone_game_state(game_state_t* original);
    
    /**
     * Free cloned game state
     */
    void free_game_state_clone(game_state_t* clone);
    
    /**
     * Thread-safe move evaluation using sequential minimax
     */
    int evaluate_move_sequential(game_state_t* game, int x, int y, int depth, 
                                int alpha, int beta, int ai_player);
    
    ThreadPool thread_pool_;
};

/**
 * Global parallel AI instance (singleton pattern for C compatibility)
 */
extern std::unique_ptr<ParallelAI> g_parallel_ai;

/**
 * Initialize global parallel AI
 * @param num_threads Number of threads (0 = auto-detect)
 */
void init_parallel_ai(size_t num_threads = 0);

/**
 * Cleanup global parallel AI
 */
void cleanup_parallel_ai();

/**
 * C-compatible parallel AI search function
 */
extern "C" {
    void find_best_ai_move_parallel(game_state_t* game, int* best_x, int* best_y);
}

} // namespace gomoku