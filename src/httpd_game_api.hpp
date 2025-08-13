//
//  httpd_game_api.hpp
//  gomoku-httpd - Game API for JSON serialization and AI moves
//
//  Modern C++23 JSON API wrapper for game state management
//

#pragma once

#include <expected>
#include <string>
#include <chrono>
#include <memory>

#include "json.hpp"
#include "game.h"
#include "ai.h"
#include "gomoku.hpp"

namespace gomoku::httpd {

using json = nlohmann::json;

enum class GameAPIError {
    InvalidGameState,
    InvalidMove,
    GameAlreadyFinished,
    AITimeout,
    SerializationError,
    BoardSizeMismatch
};

struct MoveRequest {
    std::string game_id;
    int board_size;
    std::vector<std::vector<std::string>> board_state;
    std::vector<json> move_history;
    json players;
    std::string current_player;
    int ai_depth = 6;
    int timeout_ms = 0;
};

struct MoveResponse {
    std::string game_id;
    json move;
    std::string game_status;
    std::vector<std::string> board_state;  // Now array of visual strings
    std::vector<json> move_history;
    int positions_evaluated = 0;
    int move_time_ms = 0;
};

class GameAPI {
public:
    explicit GameAPI(int default_depth = 6);
    ~GameAPI();
    
    std::expected<MoveResponse, GameAPIError> process_move_request(const json& request_json);
    
    json serialize_game_state(const game_state_t* game) const;
    std::expected<std::unique_ptr<game_state_t, void(*)(game_state_t*)>, GameAPIError> 
        deserialize_game_state(const json& game_json) const;
    
    static json create_empty_game(int board_size, const std::string& game_id);
    static std::string generate_game_id();
    
private:
    std::expected<MoveRequest, GameAPIError> parse_move_request(const json& request_json) const;
    std::expected<json, GameAPIError> make_ai_move(game_state_t* game) const;
    
    json serialize_move(const move_history_t& move) const;
    std::expected<move_history_t, GameAPIError> deserialize_move(const json& move_json) const;
    
    std::vector<std::string> serialize_board(int** board, int size) const;
    std::expected<void, GameAPIError> deserialize_board(
        const std::vector<std::vector<std::string>>& board_json, 
        int** board, int size) const;
    
    const char* game_api_error_to_string(GameAPIError error) const;
    
    int default_depth_;
};

} // namespace gomoku::httpd