# Source files for the gomoku executable (Modern C++23 with Player hierarchy and parallel AI)
set(GOMOKU_SOURCES
    main.cpp
    gomoku.cpp
    board.cpp
    game.cpp
    ai.cpp
    ui.cpp
    cli.cpp
    player.cpp
    ai_parallel.cpp
    game_coordinator.cpp
    game_history.cpp
)

# Source files for the HTTP daemon
set(HTTPD_SOURCES
    httpd_main.cpp
    httpd_server.cpp
    httpd_cli.cpp
    httpd_game_api.cpp
    gomoku.cpp
    board.cpp
    ai.cpp
    game.cpp
)

# Create the gomoku executable
add_executable(gomoku ${GOMOKU_SOURCES})

# Create the gomoku-httpd executable  
add_executable(gomoku-httpd ${HTTPD_SOURCES})

# Find pthread
find_package(Threads REQUIRED)

# Link libraries
target_link_libraries(gomoku ${MATH_LIB} Threads::Threads)
target_link_libraries(gomoku-httpd ${MATH_LIB} Threads::Threads)

# Include directories
target_include_directories(gomoku PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
target_include_directories(gomoku-httpd PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Set output directory to bin folder (same as Makefile)
set_target_properties(gomoku PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
set_target_properties(gomoku-httpd PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)