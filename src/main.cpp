//
//  main.cpp
//  gomoku - Main game orchestrator
//
//  Modern C++23 main function that orchestrates the game using modular components
//

#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include "gomoku.hpp"
#include "game_coordinator.hpp"
#include "ui.hpp"
#include "cli.hpp"

int main(int argc, char* argv[]) {
    // Initialize random seed for first move randomization
    std::srand(std::time(nullptr));

    try {
        // Parse command line arguments
        cli_config_t config = parse_arguments(argc, argv);

        // Handle help or invalid arguments
        if (config.show_help) {
            print_help(argv[0]);
            return 0;
        }

        if (!validate_config(&config)) {
            print_help(argv[0]);
            return 1;
        }

        clear_screen();

        if (!config.skip_welcome) {
            draw_game_header();
        }

        // Create and run game using modern coordinator
        gomoku::GameCoordinator coordinator(config);
        return coordinator.run_game();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred" << std::endl;
        return 1;
    }
}