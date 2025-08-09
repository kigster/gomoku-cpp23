//
//  player.hpp
//  gomoku - Player abstraction for different player types
//
//  Abstract Player hierarchy supporting human and computer players
//

#pragma once

#include <string>
#include <memory>
#include <optional>
#include "gomoku.hpp"
#include "game_state_wrapper.hpp"

namespace gomoku {

// GameStateWrapper is now defined in game_state_wrapper.hpp

//===============================================================================
// MOVE RESULT STRUCTURE
//===============================================================================

/**
 * Result of a player's move attempt
 */
struct PlayerMoveResult {
    int x, y;                    // Coordinates of the move
    bool valid;                  // Whether the move is valid
    double time_taken;           // Time taken to make the move
    int positions_evaluated;     // For AI: number of positions evaluated
    std::string description;     // Optional description of the move
    
    static PlayerMoveResult invalid() {
        return {-1, -1, false, 0.0, 0, "Invalid move"};
    }
    
    static PlayerMoveResult valid_move(int x, int y, double time, int positions = 1, const std::string& desc = "") {
        return {x, y, true, time, positions, desc};
    }
};

//===============================================================================
// ABSTRACT PLAYER BASE CLASS
//===============================================================================

/**
 * Abstract base class for all player types
 */
class PlayerImpl {
public:
    PlayerImpl(const std::string& name, PlayerType type) 
        : name_(name), type_(type) {}
    
    virtual ~PlayerImpl() = default;
    
    // Pure virtual function - must be implemented by derived classes
    virtual PlayerMoveResult make_move(GameStateWrapper* game_state) = 0;
    
    // Virtual functions with default implementations
    virtual void on_game_start(GameStateWrapper* game_state) {}
    virtual void on_game_end(GameStateWrapper* game_state) {}
    virtual void on_opponent_move(GameStateWrapper* game_state, int x, int y) {}
    
    // Getters/Setters
    const std::string& get_name() const { return name_; }
    PlayerType get_type() const { return type_; }
    void set_name(const std::string& name) { name_ = name; }
    
protected:
    std::string name_;
    PlayerType type_;
};

//===============================================================================
// PLAYER TYPES
//===============================================================================

/**
 * Human player implementation
 */
class HumanPlayer : public PlayerImpl {
public:
    explicit HumanPlayer(const std::string& name = "Human") 
        : PlayerImpl(name, PlayerType::Human) {}
    
    PlayerMoveResult make_move(GameStateWrapper* game_state) override;
    
private:
    void handle_input(GameStateWrapper* game_state, int& x, int& y, bool& move_made);
};

/**
 * Computer player implementation
 */
class ComputerPlayer : public PlayerImpl {
public:
    enum class Difficulty {
        Easy = 2,    // depth 2
        Medium = 4,  // depth 4  
        Hard = 6     // depth 6
    };
    
    explicit ComputerPlayer(const std::string& name = "Computer", 
                           Difficulty difficulty = Difficulty::Medium)
        : PlayerImpl(name, PlayerType::Computer), difficulty_(difficulty) {}
    
    PlayerMoveResult make_move(GameStateWrapper* game_state) override;
    
    void on_game_start(GameStateWrapper* game_state) override;
    Difficulty get_difficulty() const { return difficulty_; }
    void set_difficulty(Difficulty difficulty) { difficulty_ = difficulty; }
    
private:
    Difficulty difficulty_;
};

//===============================================================================
// PLAYER FACTORY
//===============================================================================

/**
 * Factory class for creating players
 */
class PlayerFactory {
public:
    enum class PlayerSpec {
        Human,
        Computer
    };
    
    static std::unique_ptr<PlayerImpl> create_player(PlayerSpec spec, 
                                                     const std::string& name = "",
                                                     ComputerPlayer::Difficulty difficulty = ComputerPlayer::Difficulty::Medium);
    
    static std::pair<std::unique_ptr<PlayerImpl>, std::unique_ptr<PlayerImpl>> 
    create_player_pair(PlayerSpec player1, PlayerSpec player2);
    
private:
    static std::string get_classical_name(ComputerPlayer::Difficulty difficulty);
    static std::string generate_default_name(PlayerSpec spec, int player_number);
};

} // namespace gomoku