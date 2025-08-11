## Gomoku C++23 Version 3.0

This outlines the desired set of features for the Gomoku C++23 project. 

Right now the project is a single executable and remembers the state of the game.

I would like to add a flag to it (or build another binary) that retains all the AI algorithm for finding the best next move using minimax with alpha/beta pruning, but works in a stateless fashion. Let me explain.

Let's call this new binary `gomokud` for "daemon". This binary has several flags:

TCP/IP endpoint to bind to:

-p | --port (default 5500)
-h | --host (default 0.0.0.0)
-t | --threads (default # of available hardware processors - 1)
-d | --depth N (default 6)

This binary boots and becomes a UNIX daemon. It detaches from the TTY, makes PID 1 it's parent, and then binds to the TCP/IP socket, and starts listening for HTTP requests.

It responds to several URIs:

1. `GET /ai/v1/status HTTP/1.1`

  Returns JSON representing the load average on the machine, total idle CPU, total free RAM, and ideally the size of the current process, and number of threads.

  If the server is unhealthy this should return status code 500 instead of 200.

2. The only other HTTP endpoint supported is the one that receives a JSON serialized state of the game, including the entire move history, with the opponent's last move already placed and visibile in the history. The job of this process is to run multi-threaded AI up to the depth specificied on the command line (but can be overridden in the incoming JSON) and return back similar JSON structure (comforming to the same JSON schema) with the next move placed on the board, and (in case it's a winning move), also the change in game state.

  `POST /ai/v1/move HTTP/1.1`

  If the incominig boarrd is in an invalid state (such as the game is already finished, or perhaps does not validate against JSON schema), the response should be a JSON structure of the following type:

  The JSON must include:

   * it should describe the opponent as X or O => { "nickname": "kig", "type": "human" } (or can be "ai") — it's not really important, but it will be sent back in the response as is.
   * the game should provide the description for itself: X or O => { "nickname": "gomoku-cpp23", "type": "ai" }
   * the entire move history from the first move by X to the last. X always moves first. But that could be us: we could receive an empty board which means we are starting the game as X. Each move must include not just the duration it took, and how many moves the algorithm scanned, but also just the timestamp when it started, finished, and whether this was a winning move and game is over.
   * game status: "in progress" or "game over! X wins!"
   * the size of the board (15 or 19)

  You are free to design this JSON payload and the JSON schema associated  with it as you see fit. Whatever is the most convenient.

3. GET /gomoku.schema.json

Returns JSON schema as described on this site: https://json-schema.org/learn/getting-started-step-by-step

## JSON Schema

I suggest that we utilize some popular standards for this. For example, JSON schema project is this:

* https://json-schema.org/

This will allow us to create a schema for describing the game: the board size, the moves, the game state.

## Why?

Why are we doing this? I'd like to have various HTTP clients be able to play with this C++ server. We can run several servers behind haproxy, because each move is stateless. But that's later and not our concern right now. Just something to keep in mind. 

We are are building a multi-threaded HTTP server that responds to the above URLs. Let's also create a unit test that starts the server (non in the daemon mode for testing) and hits it with the pre-written JSON fixture, and validate the response is what we expect.

## Goal

By the end of this project, there should be a binary `gomoku` that plays as before in the Terminal, `gomoku_tests` as before, and `gomoku-httpd` — a gomoku linked with the lightest and fastest HTTP parsing library in C++ available on the internet. Same with JSON — please find the most easy to use and one that uses  the most advanced features from C++23.




