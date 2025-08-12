# Find or build Google Test
find_package(GTest QUIET)

if (NOT GTest_FOUND)
    # Check if Google Test is available in the googletest subdirectory
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/googletest)
        message(STATUS "Using local Google Test from tests/googletest")

        # Add Google Test subdirectory but don't install it
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

        add_subdirectory(googletest EXCLUDE_FROM_ALL)

        # Note: GTest::gtest and GTest::gtest_main aliases are created automatically by googletest
    else ()
        message(FATAL_ERROR "Google Test not found. Please run 'git submodule update --init' to download it.")
    endif ()
endif ()

# Source files for the test (excluding main.cpp and ui.cpp which are not needed for testing)
set(TEST_SOURCES
        gomoku_test.cpp
        ../src/gomoku.cpp
        ../src/board.cpp
        ../src/game.cpp
        ../src/ai.cpp
)

# Source files for the HTTP daemon test
set(HTTPD_TEST_SOURCES
        httpd_test.cpp
        ../src/httpd_cli.cpp
        ../src/httpd_game_api.cpp
        ../src/httpd_server.cpp
        ../src/gomoku.cpp
        ../src/board.cpp
        ../src/ai.cpp
        ../src/game.cpp
)

# Create the test executables
add_executable(test-gomoku ${TEST_SOURCES})
add_executable(test-gomoku-httpd ${HTTPD_TEST_SOURCES})

# Find pthread
find_package(Threads REQUIRED)

# Link with Google Test and math library
target_link_libraries(test-gomoku
        GTest::gtest
        GTest::gtest_main
        ${MATH_LIB}
        Threads::Threads
)
target_link_libraries(test-gomoku-httpd
        GTest::gtest
        GTest::gtest_main
        ${MATH_LIB}
        Threads::Threads
)

# Include directories
target_include_directories(test-gomoku PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_SOURCE_DIR}/include
)
target_include_directories(test-gomoku-httpd PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        ${CMAKE_SOURCE_DIR}/include
)

# Set output directory to bin folder (same as Makefile)
set_target_properties(test-gomoku PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
set_target_properties(test-gomoku-httpd PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# Add tests to CTest
add_test(NAME gomoku_tests COMMAND test-gomoku)
add_test(NAME httpd_tests COMMAND test-gomoku-httpd)
