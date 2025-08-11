# C++23 Features in the Gomoku Codebase

This document catalogs the major C++23 language features and their usage in this modern Gomoku implementation.

## Table of Contents

1. [Concepts and Constraints](#concepts-and-constraints)
2. [std::expected](#stdexpected)
3. [std::format](#stdformat)
4. [std::string_view](#stdstring_view)
5. [std::span](#stdspan)
6. [Enhanced constexpr](#enhanced-constexpr)
7. [Template and Type Deduction Improvements](#template-and-type-deduction-improvements)
8. [enum class Improvements](#enum-class-improvements)
9. [RAII and Smart Memory Management](#raii-and-smart-memory-management)
10. [Modern Function Features](#modern-function-features)
11. [Other C++23 Features Used](#other-c23-features-used)

---

## Concepts and Constraints

**C++23 Feature**: Concepts allow constraining template parameters with readable requirements.

### Usage in Codebase

**File**: `src/gomoku.hpp`
```cpp
// Board size concept
template<int Size>
concept ValidBoardSize = (Size == 15 || Size == 19);

// Coordinate concept
template<typename T>
concept Coordinate = std::integral<T> && std::signed_integral<T>;
```

**File**: `src/board.hpp`
```cpp
template<int Size = DEFAULT_BOARD_SIZE> requires ValidBoardSize<Size>
class Board {
    static constexpr int size = Size;
    static constexpr int total_cells = Size * Size;
    // ...
};
```

**File**: `src/gomoku.cpp`
```cpp
template<int Size> requires ValidBoardSize<Size>
Board<Size> create_board_modern() {
    return Board<Size>{};
}

template<int Size> requires ValidBoardSize<Size>
int evaluate_position_modern(const Board<Size>& board, Player player) {
    // Template function with concept constraint
}
```

**How It's Used**: 
- Ensures only valid board sizes (15 or 19) are accepted at compile time
- Provides clear, readable constraints replacing SFINAE
- Template functions are constrained to prevent invalid instantiation

---

## std::expected

**C++23 Feature**: `std::expected<T, E>` provides a type-safe way to return either a value or an error without exceptions.

### Usage in Codebase

**File**: `src/cli.cpp`
```cpp
std::expected<void, std::string> Config::validate() const {
    if (board_size != 15 && board_size != 19) {
        return std::unexpected("Board size must be 15 or 19");
    }
    if (search_depth < 1 || search_depth > MAX_DEPTH) {
        return std::unexpected("Search depth must be between 1 and " + 
                              std::to_string(MAX_DEPTH));
    }
    return {}; // Success
}

std::expected<Config, ParseError> parse_arguments_modern(std::span<const char*> args) {
    Config config;
    
    // ... parsing logic ...
    
    if (auto validation = config.validate(); !validation) {
        return std::unexpected(ParseError::InvalidConfig);
    }
    
    return config;
}

std::expected<std::pair<PlayerConfig, PlayerConfig>, ParseError> 
parse_players_string(std::string_view players_str) {
    // Lambda returning expected
    auto parse_single_player = [](std::string_view player_str) 
        -> std::expected<PlayerConfig, ParseError> {
        
        if (player_str.empty()) {
            return std::unexpected(ParseError::InvalidPlayer);
        }
        
        // ... parsing logic ...
        
        return PlayerConfig{type, name, difficulty};
    };
    
    // Use expected results
    auto player1 = parse_single_player(player1_str);
    if (!player1) return std::unexpected(player1.error());
    
    return std::make_pair(*player1, *player2);
}
```

**File**: `src/gomoku.hpp`
```cpp
template<typename T>
using Result = std::expected<T, std::string>;

using VoidResult = Result<void>;
using MoveResult = Result<Position>;
```

**How It's Used**:
- **Error Handling**: Replace exception-based error handling with explicit error types
- **Configuration Validation**: Validate user input and return specific error messages
- **Parser Functions**: CLI argument parsing returns errors instead of throwing
- **Type Safety**: Compiler enforces checking of error conditions

---

## std::format

**C++23 Feature**: Type-safe, efficient string formatting replacing C-style printf.

### Usage in Codebase

**File**: `src/cli.cpp`
```cpp
void print_help_modern(std::string_view program_name) {
    std::cout << std::format("\n{}NAME{}\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
    std::cout << std::format("  {} - an entertaining and engaging five-in-a-row version\n\n", 
                            program_name);
    
    std::cout << std::format("{}FLAGS:{}\n", COLOR_BRIGHT_MAGENTA, COLOR_RESET);
    std::cout << std::format("  {}-d, --depth N{}         The depth of search in the MiniMax algorithm\n", 
                            COLOR_BRIGHT_YELLOW, COLOR_RESET);
    
    // Conditional formatting with validation warnings
    std::cout << std::format("{}{}WARNING: Search at or above depth {} may be slow{}\n",
                            COLOR_BRIGHT_RED, ESCAPE_CODE_BOLD, 
                            DEPTH_WARNING_THRESHOLD, COLOR_RESET);
}
```

**File**: `src/ui.cpp`
```cpp
void draw_game_header() {
    std::cout << std::format(" {}{} {}(v{}{})\n\n",
                            COLOR_YELLOW, GAME_DESCRIPTION, 
                            COLOR_RED, GAME_VERSION, COLOR_RESET);
    
    std::cout << std::format(" {}{}{}\n\n",
                            COLOR_BRIGHT_GREEN, GAME_COPYRIGHT, COLOR_RESET);
}

void draw_status(const game_state_t* game) {
    // Formatted position display
    auto position_str = std::format("Position       : [ {:2d}, {:2d} ]",
                                   ::board_to_display_coord(game->cursor_x),
                                   ::board_to_display_coord(game->cursor_y));

    // Dynamic difficulty formatting
    auto difficulty_str = std::format("{}Difficulty     : {}", 
                                     difficulty_color, difficulty_name);
    
    // Time summary with floating point precision
    auto time_summary = std::format("Time: Human: {:.1f}s | AI: {:.1f}s",
                                   game->total_human_time, game->total_ai_time);
}
```

**File**: `src/gomoku.hpp`
```cpp
// Custom formatter for Position type
template<>
struct std::formatter<gomoku::Position> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const gomoku::Position& pos, std::format_context& ctx) const {
        return std::format_to(ctx.out(), "({}, {})", pos.x, pos.y);
    }
};
```

**How It's Used**:
- **Type Safety**: Compile-time format string validation
- **Performance**: More efficient than iostream operators
- **Readability**: Cleaner syntax than C-style printf
- **Custom Types**: Formatter specialization for game types
- **Color Formatting**: Complex ANSI color code insertion

---

## std::string_view

**C++23 Feature**: Non-owning, lightweight string references avoiding unnecessary copies.

### Usage in Codebase

**File**: `src/gomoku.hpp`
```cpp
// Compile-time string constants
inline constexpr std::string_view GAME_NAME = "Gomoku";
inline constexpr std::string_view GAME_VERSION = "2.0.0";
inline constexpr std::string_view GAME_AUTHOR = "Konstantin Gredeskoul";
inline constexpr std::string_view GAME_COPYRIGHT = "© 2025 Konstantin Gredeskoul, MIT License";

// Unicode constants as string views
inline constexpr std::string_view UNICODE_CROSSES = "✕";
inline constexpr std::string_view UNICODE_NAUGHTS = "○";
inline constexpr std::string_view UNICODE_CURSOR = "✕";

// Multi-line string literals
inline constexpr std::string_view GAME_RULES_BRIEF = R"(
    Arrow Keys to move, SPACE to place stone, ? for help, ESC to quit
    Goal: Get exactly 5 in a row (horizontal, vertical, or diagonal)
)";

constexpr std::string_view player_to_unicode(Player player) noexcept {
    switch (player) {
        case Player::Cross: return UNICODE_CROSSES;
        case Player::Naught: return UNICODE_NAUGHTS;
        case Player::Empty: return symbols::EMPTY;
    }
}

std::string_view get_coordinate_unicode(int index) noexcept;
```

**File**: `src/cli.cpp`
```cpp
// Function parameters use string_view for efficiency
std::expected<std::pair<PlayerConfig, PlayerConfig>, ParseError> 
parse_players_string(std::string_view players_str) {
    
    auto comma_pos = players_str.find(',');
    if (comma_pos == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidFormat);
    }
    
    // Substring operations create views, not copies
    std::string_view player1_str = players_str.substr(0, comma_pos);
    std::string_view player2_str = players_str.substr(comma_pos + 1);
    
    // Lambda captures string_view by value efficiently
    auto parse_single_player = [](std::string_view player_str) 
        -> std::expected<PlayerConfig, ParseError> {
        // Process without copying the string
        auto colon_pos = player_str.find(':');
        if (colon_pos != std::string_view::npos) {
            std::string_view type = player_str.substr(0, colon_pos);
            std::string_view rest = player_str.substr(colon_pos + 1);
            // ...
        }
    };
}

void print_help_modern(std::string_view program_name) {
    using namespace std::string_view_literals;  // Enable ""sv literals
    
    // Efficient string operations without allocation
    std::cout << std::format("  {} - an entertaining game\n", program_name);
}

std::string_view error_to_string(ParseError error) {
    using namespace std::string_view_literals;
    
    switch (error) {
        case ParseError::InvalidDepth: return "Invalid search depth"sv;
        case ParseError::InvalidBoard: return "Invalid board size"sv;
        case ParseError::InvalidLevel: return "Invalid difficulty level"sv;
        // ... returns compile-time string views
    }
}
```

**File**: `src/board.cpp`
```cpp
[[nodiscard]] std::string_view get_coordinate_unicode(int index) noexcept {
    static constexpr std::array<std::string_view, 19> COORDINATE_UNICODE = {{
        "❶", "❷", "❸", "❹", "❺", "❻", "❼", "❽", "❾", "❿",
        "❶", "❷", "❸", "❹", "❺", "❻", "❼", "❽", "❾"
    }};
    
    if (index >= 0 && index < static_cast<int>(COORDINATE_UNICODE.size())) {
        return COORDINATE_UNICODE[index];
    }
    return "?";
}
```

**How It's Used**:
- **Zero-Copy Operations**: Substring operations don't allocate memory
- **Function Parameters**: Avoid string copies in function calls
- **Compile-Time Constants**: Efficient storage of string literals
- **String Processing**: Fast parsing without temporary objects
- **Return Values**: Return string references instead of copies

---

## std::span

**C++23 Feature**: Non-owning view over contiguous memory sequences, safer than raw pointers.

### Usage in Codebase

**File**: `src/cli.cpp`
```cpp
std::expected<Config, ParseError> parse_arguments_modern(std::span<const char*> args) {
    Config config;
    
    // Safe iteration over arguments without pointer arithmetic
    for (size_t i = 0; i < args.size(); ++i) {
        const char* arg = args[i];
        // Process arguments safely
    }
    
    return config;
}

// Main function creates span from argv
int main(int argc, char* argv[]) {
    std::span<const char*> args{const_cast<const char**>(argv), 
                               static_cast<size_t>(argc)};
    
    auto result = gomoku::cli::parse_arguments_modern(args);
    if (!result) {
        // Handle parsing error
        return 1;
    }
}
```

**File**: `src/board.hpp`
```cpp
template<int Size = DEFAULT_BOARD_SIZE> requires ValidBoardSize<Size>
class Board {
public:
    // Type aliases for spans over board rows
    using Row = std::span<Player, Size>;
    using ConstRow = std::span<const Player, Size>;
    
    // Return spans instead of raw pointers/arrays
    [[nodiscard]] constexpr auto operator[](int x) noexcept -> Row {
        return std::span{board_[x]};
    }
    
    [[nodiscard]] constexpr auto operator[](int x) const noexcept -> ConstRow {
        return std::span{board_[x]};
    }

private:
    using BoardData = std::array<std::array<Player, Size>, Size>;
    BoardData board_{};
};
```

**File**: `src/gomoku.cpp`
```cpp
[[nodiscard]] ThreatType calc_threat_in_one_dimension(std::span<const Player> line, 
                                                     Player player) {
    // Safe iteration over line without bounds checking concerns
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == player) {
            // Process player stones safely
        }
    }
    
    // Algorithm works with any size line through span interface
    int consecutive = 0;
    for (const Player p : line) {
        if (p == player) {
            consecutive++;
        } else {
            consecutive = 0;
        }
        
        if (consecutive >= NEED_TO_WIN) {
            return ThreatType::Five;
        }
    }
    
    return ThreatType::None;
}
```

**How It's Used**:
- **Memory Safety**: Bounds-checked access to contiguous memory
- **Generic Algorithms**: Functions work with any contiguous container
- **Interface Safety**: Replace raw pointers with bounded views
- **Board Access**: Safe row access returning spans instead of pointers
- **Command Line**: Safe argument processing without pointer arithmetic

---

## Enhanced constexpr

**C++23 Feature**: Extended constexpr capabilities allowing more complex compile-time computations.

### Usage in Codebase

**File**: `src/gomoku.hpp`
```cpp
// Compile-time constants
inline constexpr int DEFAULT_BOARD_SIZE = 19;
inline constexpr int SEARCH_RADIUS = 4;
inline constexpr int NEED_TO_WIN = 5;
inline constexpr int NUM_DIRECTIONS = 4;

// Constexpr arrays for compile-time direction calculations
inline constexpr std::array<Position, NUM_DIRECTIONS> DIRECTIONS = {{
    {1, 0},   // Horizontal
    {0, 1},   // Vertical  
    {1, 1},   // Diagonal
    {1, -1}   // Anti-diagonal
}};

// Complex constexpr functions
constexpr Player other_player(Player player) noexcept {
    return (player == Player::Cross) ? Player::Naught : Player::Cross;
}

constexpr std::string_view player_to_unicode(Player player) noexcept {
    switch (player) {
        case Player::Cross: return UNICODE_CROSSES;
        case Player::Naught: return UNICODE_NAUGHTS; 
        case Player::Empty: return symbols::EMPTY;
    }
}
```

**File**: `src/board.hpp`
```cpp
template<int Size = DEFAULT_BOARD_SIZE> requires ValidBoardSize<Size>
class Board {
private:
    using BoardData = std::array<std::array<Player, Size>, Size>;
    BoardData board_{};

public:
    // Compile-time constants
    static constexpr int size = Size;
    static constexpr int total_cells = Size * Size;
    
    // Constexpr constructor enables compile-time board creation
    constexpr Board() noexcept {
        clear();
    }
    
    // All board operations can be evaluated at compile time
    [[nodiscard]] constexpr Player at(const Position& pos) const noexcept {
        return board_[pos.x][pos.y];
    }
    
    constexpr void set(const Position& pos, Player player) noexcept {
        if (is_valid_position(pos)) {
            board_[pos.x][pos.y] = player;
        }
    }
    
    constexpr void clear() noexcept {
        for (auto& row : board_) {
            for (auto& cell : row) {
                cell = Player::Empty;
            }
        }
    }
    
    // Complex constexpr algorithms
    [[nodiscard]] constexpr bool has_winner(Player player) const noexcept {
        for (int x = 0; x < size; ++x) {
            for (int y = 0; y < size; ++y) {
                if (at(x, y) == player && check_win_from_position({x, y}, player)) {
                    return true;
                }
            }
        }
        return false;
    }
    
    [[nodiscard]] constexpr int count_line(const Position& start, 
                                          const Position& direction, 
                                          Player player) const noexcept {
        int count = 0;
        Position pos = start;
        
        while (is_valid_position(pos) && at(pos) == player) {
            count++;
            pos = pos + direction;
        }
        
        return count;
    }
    
    // Compile-time coordinate conversion
    [[nodiscard]] static constexpr int board_to_display_coord(int board_coord) noexcept {
        return board_coord + 1;
    }
    
    [[nodiscard]] static constexpr int display_to_board_coord(int display_coord) noexcept {
        return display_coord - 1;
    }
};
```

**File**: `src/gomoku.cpp`
```cpp
namespace detail {
    // Compile-time conversion functions
    constexpr Player int_to_player(int player) noexcept {
        switch (player) {
            case 1: return Player::Cross;
            case -1: return Player::Naught;
            default: return Player::Empty;
        }
    }
    
    constexpr int player_to_int(Player player) noexcept {
        switch (player) {
            case Player::Cross: return 1;
            case Player::Naught: return -1;
            case Player::Empty: return 0;
        }
    }
}
```

**How It's Used**:
- **Compile-Time Calculation**: Game constants computed at compile time
- **Template Specialization**: Board size calculations at compile time  
- **Algorithm Optimization**: Win detection algorithms can run at compile time
- **Zero Runtime Cost**: Complex calculations moved from runtime to compile time
- **Type Safety**: Constexpr functions enforce compile-time correctness

---

## Template and Type Deduction Improvements

**C++23 Feature**: Enhanced template argument deduction and abbreviated function templates.

### Usage in Codebase

**File**: `src/board.hpp`
```cpp
// Template with concept constraints
template<int Size = DEFAULT_BOARD_SIZE> requires ValidBoardSize<Size>
class Board {
    // Auto return type deduction with constraints
    [[nodiscard]] constexpr auto operator[](int x) noexcept -> Row {
        return std::span{board_[x]};  // Template argument deduction
    }
    
    [[nodiscard]] constexpr auto operator[](int x) const noexcept -> ConstRow {
        return std::span{board_[x]};  // Deduced const version
    }
    
    // Auto deduction for iterators
    [[nodiscard]] constexpr auto begin() noexcept { return board_.begin(); }
    [[nodiscard]] constexpr auto end() noexcept { return board_.end(); }
    [[nodiscard]] constexpr auto begin() const noexcept { return board_.begin(); }
    [[nodiscard]] constexpr auto end() const noexcept { return board_.end(); }
};

// Type aliases with template deduction
using Board15 = Board<15>;
using Board19 = Board<19>;
using StandardBoard = Board<DEFAULT_BOARD_SIZE>;
```

**File**: `src/cli.cpp`
```cpp
std::expected<std::pair<PlayerConfig, PlayerConfig>, ParseError> 
parse_players_string(std::string_view players_str) {
    
    // Generic lambda with auto parameters and return type deduction
    auto parse_single_player = [](std::string_view player_str) 
        -> std::expected<PlayerConfig, ParseError> {
        
        // Auto type deduction for string position
        auto colon_pos = player_str.find(':');
        if (colon_pos != std::string_view::npos) {
            // Type deduction for substring results
            auto type = player_str.substr(0, colon_pos);
            auto rest = player_str.substr(colon_pos + 1);
            
            // Further auto deduction for nested parsing
            auto colon2_pos = rest.find(':');
            if (colon2_pos != std::string_view::npos) {
                auto name = rest.substr(0, colon2_pos);
                auto difficulty = rest.substr(colon2_pos + 1);
                return PlayerConfig{std::string{type}, std::string{name}, 
                                  std::string{difficulty}};
            }
        }
    };
    
    // Auto deduction for expected results
    auto player1 = parse_single_player(player1_str);
    auto player2 = parse_single_player(player2_str);
    
    if (!player1) return std::unexpected(player1.error());
    if (!player2) return std::unexpected(player2.error());
    
    return std::make_pair(*player1, *player2);
}
```

**File**: `src/util/thread_pool.hpp`
```cpp
template<class F, class... Args>
auto enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result_t<F, Args...>> {
    
    // Type deduction for return type
    using return_type = typename std::invoke_result_t<F, Args...>;
    
    // Auto deduction for shared pointer to packaged task
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    // Auto deduction for future type
    auto result = task->get_future();
    
    return result;
}
```

**How It's Used**:
- **Auto Return Types**: Functions deduce return types automatically
- **Template Argument Deduction**: Constructors deduce template parameters
- **Lambda Auto Parameters**: Generic lambdas with type deduction
- **Iterator Deduction**: Container iterators deduced automatically
- **Complex Type Simplification**: Nested template types simplified with auto

---

## enum class Improvements

**C++23 Feature**: Scoped enumerations with improved underlying type control and safety.

### Usage in Codebase

**File**: `src/gomoku.hpp`
```cpp
// Strongly typed enums with specific underlying types for memory efficiency
enum class Player : int8_t {
    Empty = 0,
    Cross = 1,   // Human player (X)
    Naught = -1  // AI player (O)
};

enum class PlayerType : uint8_t {
    Human = 0,
    Computer = 1
};

enum class GameState : uint8_t {
    Running = 0,
    HumanWin = 1,
    AIWin = 2, 
    Draw = 3,
    Quit = 4
};

enum class ThreatType : uint8_t {
    None = 0,
    Two = 1,
    Three = 2,
    BrokenThree = 3,
    Four = 4,
    BrokenFour = 5,
    Five = 6
};

enum class Difficulty : uint8_t {
    Easy = 2,    // Search depth 2
    Medium = 4,  // Search depth 4
    Hard = 6     // Search depth 6
};
```

**File**: `src/ui.hpp`
```cpp
enum class Key : int {
    Up = 72,
    Down = 80,
    Left = 75,
    Right = 77,
    Enter = 13,
    Space = 32,
    Escape = 27,
    Tab = 9
};
```

**File**: `src/player.hpp`
```cpp
enum class Difficulty {
    Easy,
    Medium, 
    Hard
};

enum class PlayerSpec {
    Human,
    Computer
};
```

**File**: `src/cli.hpp`
```cpp
enum class ParseError {
    None,
    InvalidDepth,
    InvalidBoard,
    InvalidLevel,
    InvalidTimeout,
    InvalidPlayer,
    InvalidThreads,
    UnknownOption,
    MissingArgument,
    InvalidConfig
};
```

**Usage Examples**:
```cpp
// Type-safe comparisons - no implicit conversions
void handle_input(game_state_t* game) const {
    int key = get_key();
    
    switch (key) {
        case static_cast<int>(Key::Left):
            if (game->cursor_x > 0) game->cursor_x--;
            break;
        case static_cast<int>(Key::Right):
            if (game->cursor_x < game->board_size - 1) game->cursor_x++;
            break;
        case static_cast<int>(Key::Up):
            if (game->cursor_y > 0) game->cursor_y--;
            break;
        case static_cast<int>(Key::Down):
            if (game->cursor_y < game->board_size - 1) game->cursor_y++;
            break;
    }
}

// Safe game state transitions
void update_game_state(GameState new_state) {
    // Compiler prevents mixing different enum types
    game->game_state = static_cast<int>(new_state);
}

// Type-safe player operations
constexpr Player other_player(Player player) noexcept {
    return (player == Player::Cross) ? Player::Naught : Player::Cross;
}

constexpr std::string_view player_to_unicode(Player player) noexcept {
    switch (player) {
        case Player::Cross: return UNICODE_CROSSES;
        case Player::Naught: return UNICODE_NAUGHTS;
        case Player::Empty: return symbols::EMPTY;
    }
}
```

**How It's Used**:
- **Type Safety**: Prevents mixing different enumeration types
- **Memory Optimization**: Specific underlying types reduce memory usage  
- **Namespace Safety**: Scoped names avoid global namespace pollution
- **Clear Intent**: Enum names clearly indicate their purpose and values
- **Compile-Time Safety**: Wrong enum usage caught at compile time

---

## RAII and Smart Memory Management

**C++23 Feature**: Resource Acquisition Is Initialization with modern smart pointers and containers.

### Usage in Codebase

**File**: `src/board.hpp`
```cpp
template<int Size = DEFAULT_BOARD_SIZE> requires ValidBoardSize<Size>
class Board {
private:
    // RAII: Automatic memory management with std::array
    using BoardData = std::array<std::array<Player, Size>, Size>;
    BoardData board_{};  // Automatically initialized and cleaned up

public:
    // Constructor automatically initializes all resources
    constexpr Board() noexcept {
        clear();  // Initialize to empty state
    }
    
    // Destructor automatically called - no manual cleanup needed
    ~Board() = default;  // Compiler generates optimal destructor
    
    // Move/copy operations handled automatically by std::array
    Board(const Board&) = default;
    Board& operator=(const Board&) = default;
    Board(Board&&) = default;
    Board& operator=(Board&&) = default;
    
    // No raw pointers - all access through safe interfaces
    [[nodiscard]] constexpr auto operator[](int x) noexcept -> Row {
        return std::span{board_[x]};  // Returns safe view, not raw pointer
    }
};
```

**File**: `src/ui.cpp`
```cpp
namespace gomoku::ui {

class TerminalInput {
public:
    TerminalInput() {
        enable_raw_mode();  // Acquire terminal resource
    }
    
    ~TerminalInput() {
        if (raw_mode_enabled_) {
            disable_raw_mode();  // Automatically release resource
        }
    }

    // Prevent copying to avoid double-cleanup
    TerminalInput(const TerminalInput&) = delete;
    TerminalInput& operator=(const TerminalInput&) = delete;
    TerminalInput(TerminalInput&&) = delete;
    TerminalInput& operator=(TerminalInput&&) = delete;

private:
    struct termios original_termios_;
    bool raw_mode_enabled_ = false;
    
    void enable_raw_mode();
    void disable_raw_mode();
};

} // namespace gomoku::ui
```

**File**: `src/util/thread_pool.hpp`
```cpp
class ThreadPool {
private:
    // RAII containers manage thread lifetime
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    // Thread-safe synchronization primitives
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;

public:
    explicit ThreadPool(size_t threads = std::max(1u, std::thread::hardware_concurrency() - 1))
        : stop_(false) {
        
        // RAII: Reserve space and construct threads
        workers_.reserve(threads);
        for (size_t i = 0; i < threads; ++i) {
            workers_.emplace_back([this] {
                // Thread worker loop with RAII-based cleanup
                for (;;) {
                    std::function<void()> task;
                    
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        
                        if (stop_ && tasks_.empty()) {
                            return;  // Automatic cleanup on scope exit
                        }
                        
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    
                    task();  // Exception safe execution
                }
            });
        }
    }
    
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        
        condition_.notify_all();
        
        // RAII: Automatically join all threads
        for (std::thread& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        // std::vector automatically cleans up thread objects
    }
    
    // Non-copyable, non-movable for safety
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;
};
```

**File**: `src/ai_parallel.cpp`
```cpp
game_state_t* ParallelAI::clone_game_state(game_state_t* original) {
    // Smart pointer would be better, but using RAII principles
    // with proper cleanup in corresponding free function
    
    game_state_t* clone = static_cast<game_state_t*>(std::malloc(sizeof(game_state_t)));
    if (!clone) {
        return nullptr;  // Safe error handling
    }
    
    // Copy all data safely
    std::memcpy(clone, original, sizeof(game_state_t));
    
    // Allocate board memory with proper error checking
    clone->board = static_cast<int**>(std::malloc(original->board_size * sizeof(int*)));
    if (!clone->board) {
        std::free(clone);
        return nullptr;
    }
    
    for (int i = 0; i < original->board_size; i++) {
        clone->board[i] = static_cast<int*>(std::malloc(original->board_size * sizeof(int)));
        if (!clone->board[i]) {
            // Cleanup already allocated memory
            for (int j = 0; j < i; j++) {
                std::free(clone->board[j]);
            }
            std::free(clone->board);
            std::free(clone);
            return nullptr;
        }
        std::memcpy(clone->board[i], original->board[i], original->board_size * sizeof(int));
    }
    
    return clone;
}

void ParallelAI::free_game_state_clone(game_state_t* clone) {
    if (!clone) return;
    
    // RAII-style cleanup in reverse order of allocation
    if (clone->board) {
        for (int i = 0; i < clone->board_size; i++) {
            std::free(clone->board[i]);
        }
        std::free(clone->board);
    }
    std::free(clone);
}
```

**How It's Used**:
- **Automatic Cleanup**: Resources automatically freed when objects go out of scope
- **Exception Safety**: Resources properly cleaned up even during exceptions
- **No Memory Leaks**: RAII ensures all allocated memory is freed
- **Thread Safety**: Proper cleanup of thread resources in ThreadPool
- **Terminal State**: Raw mode automatically restored when TerminalInput destroyed

---

## Modern Function Features

**C++23 Feature**: Enhanced function capabilities including lambdas, attributes, and parameter features.

### Usage in Codebase

**File**: `src/gomoku.hpp`
```cpp
// [[nodiscard]] attribute prevents ignoring important return values
[[nodiscard]] ThreatType calc_threat_in_one_dimension(std::span<const Player> line, Player player);

[[nodiscard]] constexpr Player other_player(Player player) noexcept;

[[nodiscard]] constexpr std::string_view player_to_unicode(Player player) noexcept;

std::string_view get_coordinate_unicode(int index) noexcept;
```

**File**: `src/board.hpp`
```cpp
template<int Size = DEFAULT_BOARD_SIZE> requires ValidBoardSize<Size>
class Board {
public:
    // [[nodiscard]] on important query methods
    [[nodiscard]] constexpr Player at(const Position& pos) const noexcept;
    [[nodiscard]] constexpr bool is_empty() const noexcept;
    [[nodiscard]] constexpr bool is_full() const noexcept;
    [[nodiscard]] constexpr int stone_count() const noexcept;
    [[nodiscard]] constexpr bool is_valid_position(const Position& pos) const noexcept;
    [[nodiscard]] constexpr bool has_winner(Player player) const noexcept;
    
    // Static member functions with [[nodiscard]]
    [[nodiscard]] static constexpr char index_to_coordinate(int index) noexcept;
    [[nodiscard]] static constexpr int coordinate_to_index(char coord) noexcept;
    [[nodiscard]] static constexpr int board_to_display_coord(int board_coord) noexcept;
    
    // Defaulted special member functions
    constexpr bool operator==(const Board& other) const noexcept = default;
    constexpr auto operator<=>(const Board& other) const noexcept = default;
};
```

**File**: `src/cli.cpp`
```cpp
std::expected<std::pair<PlayerConfig, PlayerConfig>, ParseError> 
parse_players_string(std::string_view players_str) {
    
    // Generic lambda with auto parameters and trailing return type
    auto parse_single_player = [](std::string_view player_str) 
        -> std::expected<PlayerConfig, ParseError> {
        
        if (player_str.empty()) {
            return std::unexpected(ParseError::InvalidPlayer);
        }
        
        // Nested lambda for difficulty parsing
        auto parse_difficulty = [](std::string_view diff_str) -> std::string {
            // Local transformation logic
            if (diff_str == "e" || diff_str == "easy") return "easy";
            if (diff_str == "m" || diff_str == "med" || diff_str == "medium") return "medium";
            if (diff_str == "h" || diff_str == "hard") return "hard";
            return std::string{diff_str};
        };
        
        std::string type, name, difficulty;
        
        auto colon_pos = player_str.find(':');
        if (colon_pos != std::string_view::npos) {
            type = player_str.substr(0, colon_pos);
            auto rest = player_str.substr(colon_pos + 1);
            
            auto colon2_pos = rest.find(':');
            if (colon2_pos != std::string_view::npos) {
                name = rest.substr(0, colon2_pos);
                difficulty = parse_difficulty(rest.substr(colon2_pos + 1));
            } else {
                // Determine if rest is name or difficulty using lambda
                auto is_difficulty = [](std::string_view s) -> bool {
                    return s == "easy" || s == "medium" || s == "hard" || 
                           s == "e" || s == "m" || s == "h";
                };
                
                if (is_difficulty(rest)) {
                    difficulty = parse_difficulty(rest);
                } else {
                    name = rest;
                }
            }
        } else {
            type = player_str;
        }
        
        return PlayerConfig{type, name, difficulty};
    };
    
    auto comma_pos = players_str.find(',');
    if (comma_pos == std::string_view::npos) {
        return std::unexpected(ParseError::InvalidFormat);
    }
    
    auto player1_str = players_str.substr(0, comma_pos);
    auto player2_str = players_str.substr(comma_pos + 1);
    
    auto player1 = parse_single_player(player1_str);
    auto player2 = parse_single_player(player2_str);
    
    if (!player1) return std::unexpected(player1.error());
    if (!player2) return std::unexpected(player2.error());
    
    return std::make_pair(*player1, *player2);
}

// Function with [[nodiscard]] attribute  
[[nodiscard]] std::string_view error_to_string(ParseError error) {
    using namespace std::string_view_literals;
    
    switch (error) {
        case ParseError::None: return "No error"sv;
        case ParseError::InvalidDepth: return "Invalid search depth"sv;
        case ParseError::InvalidBoard: return "Invalid board size"sv;
        case ParseError::InvalidLevel: return "Invalid difficulty level"sv;
        case ParseError::InvalidTimeout: return "Invalid timeout value"sv;
        case ParseError::InvalidPlayer: return "Invalid player specification"sv;
        case ParseError::InvalidThreads: return "Invalid thread count"sv;
        case ParseError::UnknownOption: return "Unknown command line option"sv;
        case ParseError::MissingArgument: return "Missing required argument"sv;
        case ParseError::InvalidConfig: return "Invalid configuration"sv;
    }
}
```

**File**: `src/ui.cpp`
```cpp
void display_rules() {
    clear_screen();
    
    // ... display rules ...
    
    // Wait for any key press using lambda with auto parameter
    TerminalInput input;
    [[maybe_unused]] auto key = input.get_key();  // Explicitly ignore return value
}

void TerminalInput::handle_input(game_state_t* game) const {
    int key = get_key();

    switch (key) {
        case static_cast<int>(Key::Left):
            if (game->cursor_x > 0) game->cursor_x--;
            break;
        case static_cast<int>(Key::Right):
            if (game->cursor_x < game->board_size - 1) game->cursor_x++;
            break;
        // ... other cases ...
        default:
            // Ignore other keys - no action needed
            break;
    }
}
```

**File**: `src/util/thread_pool.hpp`
```cpp
template<class F, class... Args>
auto enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::invoke_result_t<F, Args...>> {
    
    using return_type = typename std::invoke_result_t<F, Args...>;
    
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    std::future<return_type> result = task->get_future();
    
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        if (stop_) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }
        
        // Lambda capturing shared_ptr for task lifetime management
        tasks_.emplace([task](){ (*task)(); });
    }
    
    condition_.notify_one();
    return result;
}

template<class F, class... Args>
void enqueue_detach(F&& f, Args&&... args) {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        if (stop_) {
            throw std::runtime_error("enqueue_detach on stopped ThreadPool");
        }
        
        tasks_.emplace(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    }
    
    condition_.notify_one();
}
```

**How It's Used**:
- **[[nodiscard]] Attribute**: Prevents ignoring important return values like game states
- **Generic Lambdas**: Auto parameters allow reusable function objects
- **Perfect Forwarding**: Template functions preserve value categories
- **Trailing Return Types**: Clear specification of complex return types  
- **[[maybe_unused]]**: Explicitly document intentionally unused variables
- **Defaulted Functions**: Compiler-generated optimal special member functions

---

## Other C++23 Features Used

### String Literal Operators

```cpp
// File: src/cli.cpp
using namespace std::string_view_literals;

std::string_view error_to_string(ParseError error) {
    switch (error) {
        case ParseError::InvalidDepth: return "Invalid search depth"sv;
        case ParseError::InvalidBoard: return "Invalid board size"sv;
        // ... using string_view literals for efficiency
    }
}
```

### Raw String Literals

```cpp
// File: src/gomoku.hpp
inline constexpr std::string_view GAME_RULES_BRIEF = R"(
    Arrow Keys to move, SPACE to place stone, ? for help, ESC to quit
    Goal: Get exactly 5 in a row (horizontal, vertical, or diagonal)
)";

inline constexpr std::string_view GAME_RULES_LONG = R"(
Rules:
1. Players alternate placing stones on board intersections
2. Human (X) always moves first
3. Win by getting exactly 5 stones in a row
4. Can be horizontal, vertical, or diagonal
5. Six or more in a row does NOT count as a win
)";
```

### Aggregate Initialization

```cpp
// File: src/gomoku.hpp
struct Position {
    int x = 0, y = 0;
    
    constexpr Position() noexcept = default;
    constexpr Position(int x_, int y_) noexcept : x(x_), y(y_) {}
    
    constexpr auto operator<=>(const Position&) const = default;
};

// Usage: Aggregate initialization
inline constexpr std::array<Position, NUM_DIRECTIONS> DIRECTIONS = {{
    {1, 0},   // Horizontal - aggregate initialization
    {0, 1},   // Vertical
    {1, 1},   // Diagonal  
    {1, -1}   // Anti-diagonal
}};
```

### Structured Bindings

```cpp
// File: src/cli.cpp (usage pattern)
auto [player1, player2] = parse_players_string(players_str).value();
// Could be used more extensively for tuple/pair returns
```

---

## Summary

This Gomoku implementation demonstrates comprehensive usage of C++23 features:

### Key Modern Features:
1. **Concepts** - Compile-time template constraints for board sizes
2. **std::expected** - Type-safe error handling without exceptions  
3. **std::format** - Type-safe, efficient string formatting
4. **std::string_view** - Zero-copy string operations
5. **std::span** - Memory-safe views over contiguous data
6. **Enhanced constexpr** - Compile-time game logic evaluation
7. **Template improvements** - Auto deduction and concept constraints
8. **enum class** - Type-safe, memory-efficient enumerations
9. **RAII** - Automatic resource management throughout
10. **Modern function features** - Attributes, lambdas, perfect forwarding

### Benefits Achieved:
- **Memory Safety**: No raw pointers, bounds checking, RAII
- **Performance**: Compile-time evaluation, zero-copy operations
- **Type Safety**: Strong typing, concept constraints, no implicit conversions
- **Maintainability**: Clear interfaces, self-documenting code, error handling
- **Modern Idioms**: Idiomatic C++23 code throughout the codebase

The codebase serves as an excellent example of how modern C++23 features can be applied to create efficient, safe, and maintainable applications while preserving performance and adding advanced functionality like parallel AI processing.