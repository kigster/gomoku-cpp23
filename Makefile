# vim: ft=make
# vim: tabstop=8
# vim: shiftwidth=8
# vim: noexpandtab

# Source environment configuration from .envrc
SHELL := /bin/bash
.SHELLFLAGS := -c 'source .envrc && exec "$$@"' --

PATH            := /usr/local/Cellar/gcc/15.1.0/bin:$(PATH)
OS              := $(shell uname -s | tr '[:upper:]' '[:lower:]')
MAKEFILE_PATH   := $(abspath $(lastword $(MAKEFILE_LIST)))
CURRENT_DIR     := $(shell ( cd .; pwd -P ) )
VERSION         := $(shell grep GAME_VERSION src/gomoku.hpp | head -1 | cut -d'"' -f2)
TAG             := $(shell echo "v$(VERSION)")
BRANCH          := $(shell git branch --show)
BIN             := $(shell echo "$(CURRENT_DIR)/bin")

CC               = gcc-15
CXX              = g++-15
CFLAGS           = -Wall -Wunused-parameter -Wextra -Wimplicit-function-declaration -Isrc -O3
CXXFLAGS         = -Wall -Wunused-parameter -Wextra -Wpedantic -std=c++23 -Isrc -Iinclude -Itests/googletest/googletest/include -O3 -march=native
LDFLAGS          = -lm -pthread

TARGET           = $(BIN)/gomoku
# Modern C++23 sources with new player hierarchy and parallel AI
CPP_SOURCES      = src/gomoku.cpp src/board.cpp src/main.cpp src/ui.cpp src/cli.cpp src/ai.cpp src/game.cpp src/player.cpp src/ai_parallel.cpp src/game_coordinator.cpp src/game_history.cpp
C_SOURCES        =
CPP_OBJECTS      = $(CPP_SOURCES:.cpp=.o)
C_OBJECTS        = $(C_SOURCES:.c=.o)
OBJECTS          = $(CPP_OBJECTS) $(C_OBJECTS)

# HTTP daemon configuration  
HTTPD_TARGET     = $(BIN)/gomoku-httpd
HTTPD_CPP_SOURCES = src/httpd_main.cpp src/httpd_server.cpp src/httpd_cli.cpp src/httpd_game_api.cpp src/gomoku.cpp src/board.cpp src/ai.cpp src/game.cpp
HTTPD_CPP_OBJECTS = $(HTTPD_CPP_SOURCES:.cpp=.o)
HTTPD_OBJECTS    = $(HTTPD_CPP_OBJECTS)

# Test configuration
TEST_TARGET      = $(BIN)/test-gomoku
TEST_CPP_SOURCES = tests/gomoku_test.cpp src/gomoku.cpp src/board.cpp src/game.cpp src/ai.cpp
TEST_C_SOURCES   = 
TEST_CPP_OBJECTS = $(TEST_CPP_SOURCES:.cpp=.o)
TEST_C_OBJECTS   = $(TEST_C_SOURCES:.c=.o)
TEST_OBJECTS     = $(TEST_CPP_OBJECTS) $(TEST_C_OBJECTS)
GTEST_LIB        = tests/googletest/build/lib/libgtest.a
GTEST_MAIN_LIB   = tests/googletest/build/lib/libgtest_main.a

# HTTP daemon test configuration
HTTPD_TEST_TARGET      = $(BIN)/test-gomoku-httpd
HTTPD_TEST_CPP_SOURCES = tests/httpd_test.cpp src/httpd_cli.cpp src/httpd_game_api.cpp src/httpd_server.cpp src/gomoku.cpp src/board.cpp src/ai.cpp src/game.cpp
HTTPD_TEST_CPP_OBJECTS = $(HTTPD_TEST_CPP_SOURCES:.cpp=.o)
HTTPD_TEST_OBJECTS     = $(HTTPD_TEST_CPP_OBJECTS)

# CMake build directory
BUILD_DIR = build

.PHONY: clean test test-httpd tag help cmake-build cmake-clean cmake-test httpd httpd-clean

help:		## Prints help message auto-generated from the comments.
		@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | \
			awk 'BEGIN {FS = ":.*?## "}; {printf "\033[32m%-20s\033[35m %s\033[0\n", $$1, $$2}' | \
			sed '/^$$/d' | \
			sort

version:        ## Prints the current version and tag
	        @echo "Version is $(VERSION)"
		@echo "The tag is $(TAG)"

build: 		$(TARGET) ## Build the Game
 
rebuild: 	clean build ## Clean and rebuild the game

httpd:		$(HTTPD_TARGET) ## Build the HTTP daemon

$(TARGET): $(OBJECTS)
		$(CXX) $(OBJECTS) $(LDFLAGS) -o $(TARGET)

$(HTTPD_TARGET): $(HTTPD_OBJECTS)
		$(CXX) $(HTTPD_OBJECTS) $(LDFLAGS) -o $(HTTPD_TARGET)

# Compilation rules for C++ files
src/%.o: src/%.cpp
		$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilation rules for C files  
src/%.o: src/%.c
		$(CC) $(CFLAGS) -c $< -o $@

$(TEST_TARGET): $(TEST_OBJECTS) # Test targets
		$(CXX) $(CXXFLAGS) $(TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) -o $(TEST_TARGET)

$(HTTPD_TEST_TARGET): $(HTTPD_TEST_OBJECTS) # HTTP daemon test targets
		$(CXX) $(CXXFLAGS) $(HTTPD_TEST_OBJECTS) $(GTEST_LIB) $(GTEST_MAIN_LIB) $(LDFLAGS) -o $(HTTPD_TEST_TARGET)

tests/gomoku_test.o: tests/gomoku_test.cpp src/gomoku.hpp src/board.hpp src/game.h
		$(CXX) $(CXXFLAGS) -c tests/gomoku_test.cpp -o tests/gomoku_test.o

tests/httpd_test.o: tests/httpd_test.cpp src/httpd_cli.hpp src/httpd_game_api.hpp src/httpd_server.hpp
		$(CXX) $(CXXFLAGS) -c tests/httpd_test.cpp -o tests/httpd_test.o

test: 		$(TEST_TARGET) $(TARGET) ## Run all the unit tests
		./$(TEST_TARGET)

test-httpd:	$(HTTPD_TEST_TARGET) ## Run HTTP daemon unit tests
		./$(HTTPD_TEST_TARGET)

clean:  	## Clean up all the intermediate objects
		rm -f $(TARGET) $(TEST_TARGET) $(HTTPD_TARGET) $(HTTPD_TEST_TARGET) $(OBJECTS) $(HTTPD_OBJECTS) $(HTTPD_TEST_OBJECTS) tests/gomoku_test.o tests/httpd_test.o
		rm -rf build
		rm -f ai_response.json

httpd-clean: ## Clean HTTP daemon objects only
		rm -f $(HTTPD_TARGET) $(HTTPD_OBJECTS)

tag:    	## Tag the current git version with the tag equal to the VERSION constant
		git tag $(TAG) -f
		git push --tags -f

release:  	tag ## Update current VERSION tag to this SHA, and publish a new Github Release
		gh release create $(TAG) --generate-notes

# CMake targets
cmake-build: 	## Build using CMake (creates build directory and runs cmake ..)
		mkdir -p $(BUILD_DIR)
		cd $(BUILD_DIR) && cmake .. && make

cmake-clean: 	## Clean CMake build directory
		rm -rf $(BUILD_DIR)

cmake-test: 	cmake-build ## Run tests using CMake
		cd $(BUILD_DIR) && ctest --verbose

cmake-rebuild: 	cmake-clean cmake-build ## Clean and rebuild using CMake

