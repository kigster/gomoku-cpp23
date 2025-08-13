# HTTPD.md

## Gomoku HTTP Daemon (`gomoku-httpd`)

This document describes the HTTP daemon functionality for the modern C++23 Gomoku game. The `gomoku-httpd` binary provides a stateless HTTP API for AI move calculations, system monitoring, and game state management.

## Overview

The HTTP daemon transforms the Gomoku AI engine into a scalable web service that can handle multiple concurrent clients. It maintains no game state between requests, making it ideal for load balancing and horizontal scaling scenarios.

### Key Features

- **Stateless Design**: Each request contains complete game state
- **Multi-threading**: Configurable thread pool for concurrent request handling
- **Modern C++23**: Built with latest C++ features and best practices
- **JSON API**: RESTful endpoints with comprehensive JSON schema validation
- **System Monitoring**: Built-in health checks and system metrics
- **Daemon Support**: Full UNIX daemon capabilities with proper signal handling
- **Testing Support**: Foreground mode for development and testing

## Building

```bash
# Build the HTTP daemon
make httpd

# Build and run tests
make test-httpd

# Clean daemon objects only
make httpd-clean
```

## Command Line Usage

```bash
# Show help
./gomoku-httpd --help

# Run in foreground (default, good for testing)
./gomoku-httpd --foreground --port 8080 --verbose

# Run as daemon
./gomoku-httpd --daemon --port 5500 --threads 8 --depth 6

# Custom configuration
./gomoku-httpd --host 192.168.1.100 --port 3000 --threads 4 --depth 4
```

### Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `-p, --port <PORT>` | TCP port to bind to | 5500 |
| `-H, --host <HOST>` | IP address to bind to | 0.0.0.0 |
| `-t, --threads <COUNT>` | Number of worker threads | CPU cores - 1 |
| `-d, --depth <DEPTH>` | AI search depth (1-10) | 6 |
| `--daemon` | Run as daemon (detach from TTY) | false |
| `--foreground` | Run in foreground (for testing) | true |
| `--verbose` | Enable verbose logging | false |
| `-h, --help` | Show help message | - |
| `-v, --version` | Show version information | - |

## HTTP API Endpoints

### 1. System Status - `GET /ai/v1/status`

Returns server health information and system metrics.

**Response Example:**

```json
{
  "status": "healthy",
  "service": "gomoku-httpd",
  "version": "3.0",
  "config": {
    "depth": 6,
    "threads": 8
  },
  "metrics": {
    "load_average": {
      "1min": 1.84,
      "5min": 2.09,
      "15min": 2.36
    },
    "memory": {
      "total_bytes": 33510535168,
      "free_bytes": 2076708864,
      "used_bytes": 31433826304,
      "free_percentage": 6.19
    },
    "process": {
      "pid": 12345,
      "ai_depth": 6,
      "threads_configured": 8
    },
    "system": {
      "cpu_cores": 20
    }
  }
}
```

### 2. AI Move Request - `POST /ai/v1/move`

Accepts a complete game state and returns the AI's best move.

**Request Headers:**

```
Content-Type: application/json
```

**Request Body:** Complete game state (see JSON Schema section). Note that `board_state` in JSON is purely for visual representation only, and is not used or parsed by the server.

**Response Example:**

```json
{
  "game_id": "test-game-12345",
  "game_status": "in_progress",
  "latest_move": {
    "player": "o",
    "position": {
      "x": 8,
      "y": 7
    },
    "timestamp": "2024-08-11T18:30:45Z",
    "move_time_ms": 245,
    "positions_evaluated": 1247,
    "is_winning_move": false
  },
  "board_state": [
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  x  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  o  •  •  •  •  • ",
    "..."
  ],
  "move_history": [...],
  "ai_metrics": {
    "move_time_ms": 245,
    "positions_evaluated": 1247
  }
}
```

### 3. JSON Schema - `GET /gomoku.schema.json`

Returns the JSON schema for game state validation. See [JSON schema fille]("../schema/gomoku.schema.json").

### 4. Health Check - `GET /health`

Simple health endpoint for load balancers.

**Response:**
```json
{
  "status": "ok",
  "service": "gomoku-httpd"
}
```

## Testing with cURL

### Prerequisites

Start the server in foreground mode for testing:

```bash
# Terminal 1: Start server
./gomoku-httpd --foreground --port 5500 --verbose

# Terminal 2: Run tests
```

### Basic Health Checks

```bash
# Simple health check
curl -s http://localhost:5500/health

# Detailed system status
curl -s http://localhost:5500/ai/v1/status | jq '.'

# Get JSON schema
curl -s http://localhost:5500/gomoku.schema.json | jq '.properties | keys'
```

### AI Move Requests

#### Using Test Fixtures

The project includes test JSON files in `tests/fixtures/` for easy testing:

```bash
# Test with the basic game state fixture
curl -s -X POST \
  -H "Content-Type: application/json" \
  -d @tests/fixtures/test_game_request.json \
  http://localhost:5500/ai/v1/move | jq '.'

# Check the game log fixture structure
jq '.' tests/fixtures/game-log-1.json
```

#### Manual Test Examples

**Empty Board (AI goes first as X):**

```bash
curl -s -X POST -H "Content-Type: application/json" \
-d '{
  "version": "1.0",
  "game": {
    "id": "empty-game-001",
    "status": "in_progress",
    "board_size": 15,
    "created_at": "2024-08-11T12:00:00Z",
    "ai_config": {"depth": 4}
  },
  "players": {
    "x": {"nickname": "gomoku-cpp23", "type": "ai"},
    "o": {"nickname": "Human", "type": "human"}
  },
  "moves": [],
  "current_player": "x",
  "board_state": [
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • "
  ]
}' \
http://localhost:5500/ai/v1/move | jq '.latest_move'
```

**Mid-Game Position (AI as O responds to X's move):**

> [!TIP]
> Note that supplying "board_state" is unnecessary since it can be definitively deduced from the sequence of moves.

```bash
curl -s -X POST -H "Content-Type: application/json" -d '{
    "version": "1.0",
    "game": {
      "id": "mid-game-001",
      "status": "in_progress",
      "board_size": 15,
      "created_at": "2024-08-11T12:00:00Z",
      "ai_config": {"depth": 6}
    },
    "players": {
      "x": {"nickname": "Human", "type": "human"},
      "o": {"nickname": "gomoku-cpp23", "type": "ai"}
    },
    "moves": [
      {
        "player": "x",
        "position": {"x": 7, "y": 7},
        "timestamp": "2024-08-11T12:00:01Z",
        "move_time_ms": 1000
      }
    ],
    "current_player": "o"
  }' http://localhost:5500/ai/v1/move
```

Response will be:

```json
{
  "ai_metrics": {
    "move_time_ms": 0,
    "positions_evaluated": 0
  },
  "board_state": [
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  o  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • ",
    " •  •  •  •  •  •  •  •  •  •  •  •  •  •  • "
  ],
  "game_id": "mid-game-001",
  "game_status": "in_progress",
  "latest_move": {
    "is_winning_move": false,
    "move_time_ms": 0,
    "player": "x",
    "position": {
      "x": 7,
      "y": 7
    },
    "positions_evaluated": 0,
    "timestamp": "2025-08-13T00:50:19.772994000Z"
  },
  "move_history": [
    {
      "move_time_ms": 1000,
      "player": "x",
      "position": {
        "x": 7,
        "y": 7
      },
      "positions_evaluated": 1,
      "timestamp": "2025-08-13T00:50:19.773007000Z"
    },
    {
      "move_time_ms": 0,
      "player": "o",
      "position": {
        "x": 7,
        "y": 7
      },
      "positions_evaluated": 0,
      "timestamp": "2025-08-13T00:50:19.773008000Z"
    }
  ]
}
```

#


```bash
# Test invalid JSON
curl -s -X POST -H "Content-Type: application/json" \
  -d '{"invalid": "json"}' \
  http://localhost:5500/ai/v1/move | jq '.'

# Test missing Content-Type
curl -s -X POST \
  -d @tests/fixtures/test_game_request.json \
  http://localhost:5500/ai/v1/move | jq '.'

# Test non-existent endpoint
curl -s http://localhost:5500/api/invalid | jq '.'
```

## JSON Schema

The complete JSON schema is available at `/gomoku.schema.json` and stored in `schema/gomoku.schema.json`. Key components:

- **Game metadata**: ID, status, board size, timestamps
- **Players**: Nicknames, types (human/ai)
- **Move history**: Complete sequence with timing and evaluation data
- **Board state**: 2D array representing current position
- **AI configuration**: Search depth, timeout settings

## Performance Notes

- **Concurrency**: The server uses a configurable thread pool for handling multiple simultaneous requests
- **Memory**: Each request creates a temporary game state that is automatically cleaned up
- **AI Depth**: Lower depth values (2-4) provide faster responses, higher values (6-8) provide stronger play
- **Scalability**: The stateless design allows running multiple instances behind a load balancer

## Integration Examples

### Node.js Client

```javascript
const axios = require('axios');

async function getAIMove(gameState) {
  try {
    const response = await axios.post('http://localhost:5500/ai/v1/move', gameState, {
      headers: { 'Content-Type': 'application/json' }
    });
    return response.data.latest_move;
  } catch (error) {
    console.error('AI move request failed:', error.response.data);
    throw error;
  }
}
```

### Python Client

```python
import requests
import json

def get_ai_move(game_state):
    response = requests.post(
        'http://localhost:5500/ai/v1/move',
        json=game_state,
        headers={'Content-Type': 'application/json'}
    )
    
    if response.status_code == 200:
        return response.json()['latest_move']
    else:
        raise Exception(f"AI move failed: {response.json()}")
```

### Load Balancing with HAProxy

```
backend gomoku_ai
    balance roundrobin
    server ai1 127.0.0.1:5500 check
    server ai2 127.0.0.1:5501 check
    server ai3 127.0.0.1:5502 check
```

## Troubleshooting

### Common Issues

1. **Port already in use**: Change port with `--port` flag
2. **Permission denied**: Use port > 1024 for non-root users
3. **Connection refused**: Check if server is running and firewall settings
4. **JSON parse errors**: Validate JSON against schema first
5. **Slow responses**: Reduce AI depth or increase timeout

### Debug Commands

```bash
# Check if server is listening
netstat -tlnp | grep 5500

# Test basic connectivity
telnet localhost 5500

# Monitor server logs (if running with --verbose)
./gomoku-httpd --foreground --verbose --port 5500

# Validate JSON against schema
curl -s http://localhost:5500/gomoku.schema.json > schema.json
# Use online JSON validator with your game state
```

## Development

### Adding New Endpoints

1. Add route handler in `src/httpd_server.cpp`
2. Update `setup_routes()` method
3. Add corresponding tests in `tests/httpd_test.cpp`
4. Update this documentation

### Modifying AI Parameters

1. Update `src/httpd_game_api.cpp` for JSON serialization
2. Modify `src/httpd_cli.hpp` for new command line options
3. Update JSON schema in `schema/gomoku.schema.json`
4. Add tests for new functionality

The HTTP daemon provides a robust, scalable interface to the Gomoku AI engine suitable for production web applications, mobile backends, and distributed gaming systems.
