//
//  httpd_game_api.cpp
//  gomoku-httpd - Game API implementation for JSON processing
//
//  Modern C++23 JSON API with comprehensive game state management
//

#include "httpd_game_api.hpp"
#include <format>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

// Game state constants (these should ideally be in a header)
#define GAME_RUNNING 0
#define GAME_WON_X 1
#define GAME_WON_O 2
#define GAME_DRAW 3

// Player constants using the modern enum
using namespace gomoku;

namespace gomoku::httpd {

GameAPI::GameAPI(int default_depth) : default_depth_(default_depth) {}

GameAPI::~GameAPI() = default;

const char* GameAPI::game_api_error_to_string(GameAPIError error) const {
    switch (error) {
        case GameAPIError::InvalidGameState:
            return "Invalid game state";
        case GameAPIError::InvalidMove:
            return "Invalid move";
        case GameAPIError::GameAlreadyFinished:
            return "Game already finished";
        case GameAPIError::AITimeout:
            return "AI move timed out";
        case GameAPIError::SerializationError:
            return "JSON serialization error";
        case GameAPIError::BoardSizeMismatch:
            return "Board size mismatch";
        default:
            return "Unknown error";
    }
}

std::string GameAPI::generate_game_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    const char* hex_chars = "0123456789abcdef";
    
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) ss << '-';
        ss << hex_chars[dis(gen)];
    }
    
    return ss.str();
}

json GameAPI::create_empty_game(int board_size, const std::string& game_id) {
    json game;
    
    game["version"] = "1.0";
    game["game"]["id"] = game_id;
    game["game"]["status"] = "in_progress";
    game["game"]["board_size"] = board_size;
    game["game"]["created_at"] = std::format("{:%Y-%m-%dT%H:%M:%SZ}", 
        std::chrono::system_clock::now());
    game["game"]["ai_config"]["depth"] = 6;
    
    game["players"]["x"]["nickname"] = "Player";
    game["players"]["x"]["type"] = "human";
    game["players"]["o"]["nickname"] = "gomoku-cpp23";
    game["players"]["o"]["type"] = "ai";
    
    game["moves"] = json::array();
    game["current_player"] = "x";
    
    // Initialize empty board as visual strings
    std::vector<std::string> board_state;
    board_state.reserve(board_size);
    
    std::string empty_row;
    empty_row.reserve(board_size * 3);
    for (int j = 0; j < board_size; ++j) {
        empty_row += " • ";
    }
    
    for (int i = 0; i < board_size; ++i) {
        board_state.push_back(empty_row);
    }
    game["board_state"] = board_state;
    
    return game;
}

std::expected<MoveResponse, GameAPIError> GameAPI::process_move_request(const json& request_json) {
    // Parse the move request
    auto request = parse_move_request(request_json);
    if (!request) {
        return std::unexpected(request.error());
    }
    
    // Create game state from JSON
    auto game_state = deserialize_game_state(request_json);
    if (!game_state) {
        return std::unexpected(game_state.error());
    }
    
    // Make AI move
    auto move_result = make_ai_move(game_state->get());
    if (!move_result) {
        return std::unexpected(move_result.error());
    }
    
    // Build response
    MoveResponse response;
    response.game_id = request->game_id;
    response.move = *move_result;
    response.board_state = serialize_board(game_state->get()->board, game_state->get()->board_size);
    
    // Serialize move history
    for (int i = 0; i < game_state->get()->move_history_count; ++i) {
        response.move_history.push_back(serialize_move(game_state->get()->move_history[i]));
    }
    
    // Determine game status
    check_game_state(game_state->get());
    switch (game_state->get()->game_state) {
        case GAME_RUNNING:
            response.game_status = "in_progress";
            break;
        case GAME_WON_X:
            response.game_status = "game_over_x_wins";
            break;
        case GAME_WON_O:
            response.game_status = "game_over_o_wins";
            break;
        case GAME_DRAW:
            response.game_status = "draw";
            break;
        default:
            response.game_status = "unknown";
    }
    
    // Get AI metrics from the last move
    if (game_state->get()->move_history_count > 0) {
        const auto& last_move = game_state->get()->move_history[game_state->get()->move_history_count - 1];
        response.positions_evaluated = last_move.positions_evaluated;
        response.move_time_ms = static_cast<int>(last_move.time_taken * 1000);
    }
    
    return response;
}

std::expected<MoveRequest, GameAPIError> GameAPI::parse_move_request(const json& request_json) const {
    try {
        MoveRequest request;
        
        if (!request_json.contains("game") || !request_json["game"].contains("id")) {
            return std::unexpected(GameAPIError::InvalidGameState);
        }
        
        request.game_id = request_json["game"]["id"];
        request.board_size = request_json["game"]["board_size"];
        
        if (request_json.contains("game") && request_json["game"].contains("ai_config")) {
            if (request_json["game"]["ai_config"].contains("depth")) {
                request.ai_depth = request_json["game"]["ai_config"]["depth"];
            }
            if (request_json["game"]["ai_config"].contains("timeout_ms")) {
                request.timeout_ms = request_json["game"]["ai_config"]["timeout_ms"];
            }
        }
        
        if (request_json.contains("board_state")) {
            request.board_state = request_json["board_state"];
        }
        
        if (request_json.contains("moves")) {
            request.move_history = request_json["moves"];
        }
        
        if (request_json.contains("players")) {
            request.players = request_json["players"];
        }
        
        if (request_json.contains("current_player")) {
            request.current_player = request_json["current_player"];
        }
        
        return request;
        
    } catch (const json::exception& e) {
        return std::unexpected(GameAPIError::SerializationError);
    }
}

std::expected<std::unique_ptr<game_state_t, void(*)(game_state_t*)>, GameAPIError> 
GameAPI::deserialize_game_state(const json& game_json) const {
    try {
        cli_config_t config = {};
        config.board_size = game_json["game"]["board_size"];
        config.max_depth = game_json.value("game", json::object()).value("ai_config", json::object()).value("depth", default_depth_);
        
        auto game = std::unique_ptr<game_state_t, void(*)(game_state_t*)>(
            init_game(config), cleanup_game);
        
        if (!game) {
            return std::unexpected(GameAPIError::InvalidGameState);
        }
        
        // Deserialize board state
        if (game_json.contains("board_state")) {
            auto deserialize_result = deserialize_board(
                game_json["board_state"], game->board, game->board_size);
            if (!deserialize_result) {
                return std::unexpected(deserialize_result.error());
            }
        }
        
        // Deserialize move history
        if (game_json.contains("moves")) {
            for (const auto& move_json : game_json["moves"]) {
                auto move_result = deserialize_move(move_json);
                if (!move_result) {
                    return std::unexpected(move_result.error());
                }
                
                // Add move to history
                add_move_to_history(game.get(), move_result->x, move_result->y, 
                                  move_result->player, move_result->time_taken,
                                  move_result->positions_evaluated);
            }
        }
        
        // Set current player
        if (game_json.contains("current_player")) {
            std::string current_player = game_json["current_player"];
            game->current_player = (current_player == "x") ? static_cast<int>(Player::Cross) : static_cast<int>(Player::Naught);
        }
        
        return game;
        
    } catch (const json::exception& e) {
        return std::unexpected(GameAPIError::SerializationError);
    }
}

std::expected<json, GameAPIError> GameAPI::make_ai_move(game_state_t* game) const {
    if (game->game_state != GAME_RUNNING) {
        return std::unexpected(GameAPIError::GameAlreadyFinished);
    }
    
    start_move_timer(game);
    
    int best_x = -1, best_y = -1;
    find_best_ai_move(game, &best_x, &best_y);
    
    double move_time = end_move_timer(game);
    
    if (best_x == -1 || best_y == -1) {
        return std::unexpected(GameAPIError::InvalidMove);
    }
    
    // Make the move
    if (!make_move(game, best_x, best_y, game->current_player, 
                   move_time, 0)) {  // positions_evaluated not available in current API
        return std::unexpected(GameAPIError::InvalidMove);
    }
    
    // Create move JSON
    json move_json;
    move_json["player"] = (game->current_player == static_cast<int>(Player::Cross)) ? "x" : "o";
    move_json["position"]["x"] = best_x;
    move_json["position"]["y"] = best_y;
    move_json["timestamp"] = std::format("{:%Y-%m-%dT%H:%M:%SZ}", 
                                       std::chrono::system_clock::now());
    move_json["move_time_ms"] = static_cast<int>(move_time * 1000);
    move_json["positions_evaluated"] = 0;  // Not available in current API
    
    // Check if this was a winning move
    check_game_state(game);
    move_json["is_winning_move"] = (game->game_state != GAME_RUNNING);
    
    return move_json;
}

json GameAPI::serialize_move(const move_history_t& move) const {
    json move_json;
    move_json["player"] = (move.player == static_cast<int>(Player::Cross)) ? "x" : "o";
    move_json["position"]["x"] = move.x;
    move_json["position"]["y"] = move.y;
    move_json["move_time_ms"] = static_cast<int>(move.time_taken * 1000);
    move_json["positions_evaluated"] = move.positions_evaluated;
    
    // Note: timestamp not available in current move_history_t structure
    move_json["timestamp"] = std::format("{:%Y-%m-%dT%H:%M:%SZ}", 
                                       std::chrono::system_clock::now());
    
    return move_json;
}

std::expected<move_history_t, GameAPIError> GameAPI::deserialize_move(const json& move_json) const {
    try {
        move_history_t move;
        
        std::string player_str = move_json["player"];
        move.player = (player_str == "x") ? static_cast<int>(Player::Cross) : static_cast<int>(Player::Naught);
        move.x = move_json["position"]["x"];
        move.y = move_json["position"]["y"];
        
        if (move_json.contains("move_time_ms")) {
            move.time_taken = move_json["move_time_ms"].get<int>() / 1000.0;
        }
        
        if (move_json.contains("positions_evaluated")) {
            move.positions_evaluated = move_json["positions_evaluated"];
        }
        
        return move;
        
    } catch (const json::exception& e) {
        return std::unexpected(GameAPIError::SerializationError);
    }
}

std::vector<std::string> GameAPI::serialize_board(int** board, int size) const {
    std::vector<std::string> board_rows;
    board_rows.reserve(size);
    
    for (int i = 0; i < size; ++i) {
        std::string row;
        row.reserve(size * 3);  // Each cell takes 3 characters
        
        for (int j = 0; j < size; ++j) {
            switch (board[i][j]) {
                case static_cast<int>(Player::Empty):
                    row += " • ";
                    break;
                case static_cast<int>(Player::Cross):
                    row += " x ";
                    break;
                case static_cast<int>(Player::Naught):
                    row += " o ";
                    break;
                default:
                    row += " • ";
            }
        }
        board_rows.push_back(row);
    }
    
    return board_rows;
}

std::expected<void, GameAPIError> GameAPI::deserialize_board(
        const std::vector<std::vector<std::string>>& board_json, 
        int** board, int size) const {
    
    if (board_json.size() != static_cast<size_t>(size)) {
        return std::unexpected(GameAPIError::BoardSizeMismatch);
    }
    
    for (int i = 0; i < size; ++i) {
        if (board_json[i].size() != static_cast<size_t>(size)) {
            return std::unexpected(GameAPIError::BoardSizeMismatch);
        }
        
        for (int j = 0; j < size; ++j) {
            const std::string& cell = board_json[i][j];
            if (cell == "empty") {
                board[i][j] = static_cast<int>(Player::Empty);
            } else if (cell == "x") {
                board[i][j] = static_cast<int>(Player::Cross);
            } else if (cell == "o") {
                board[i][j] = static_cast<int>(Player::Naught);
            } else {
                return std::unexpected(GameAPIError::InvalidGameState);
            }
        }
    }
    
    return {};
}

} // namespace gomoku::httpd