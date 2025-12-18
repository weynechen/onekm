# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

LanKM is a lightweight LAN keyboard and mouse sharing system that allows controlling two computers on a local network with a single keyboard and mouse. The system consists of:

- **Server (Linux)**: Captures physical input devices and decides where to send input
- **Client (Windows)**: Receives network input and injects it into the system

## Build Commands

```bash
# Build (Linux)
mkdir build && cd build
cmake ..
make

# Build (Windows with Visual Studio)
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Dependencies

### Linux Server Dependencies:
```bash
sudo apt-get install build-essential cmake libevdev-dev libx11-dev
```

### Windows Client Dependencies:
- Visual Studio 2015+ or MinGW
- CMake 3.15+

## Running the Applications

```bash
# Linux Server (requires root)
sudo ./build/lankm-server

# Windows Client
./build/Release/lankm-client.exe <server-ip>
```

## Architecture

### Core Components:

1. **Protocol Layer** (`src/common/`):
   - Simple binary TCP protocol for input events
   - Message struct: `{uint8_t type, int16_t a, int16_t b}`
   - Message types: mouse move, mouse button, keyboard events, control switch

2. **Server (Linux)**:
   - `input_capture.c`: Captures input using libevdev
   - `edge_detector.c`: Detects mouse screen edge for switching control
   - `state_machine.c`: Manages control state (local/remote)

3. **Client (Windows)**:
   - `input_inject.c`: Injects input using Windows API
   - `keymap.c`: Maps Linux key codes to Windows virtual key codes

### Network Architecture:
- TCP connection on port 24800
- Server listens for client connections
- Unidirectional input flow from server to client
- No GUI or configuration interface

### Key Design Decisions:
- Minimal latency (<5ms) prioritized over features
- Binary protocol for efficiency
- Platform-specific input handling
- No authentication or encryption (assumes trusted LAN)

## Development Notes

- Uses C11 standard
- Compiler warnings enabled (-Wall -Wextra -Werror)
- Struct packing for protocol compatibility
- Platform-specific code isolated in server/client directories
- No unit tests currently implemented
- Uses clang-format for code formatting (if available)

## Configuration

- TCP port: 24800 (defined in protocol.h)
- Screen edge switching: enabled by default
- No configuration file - all settings are compile-time constants

## Permissions

Linux server requires access to input devices:
```
KERNEL=="event*", MODE="0666", GROUP="input"
```

## Rules
1. 你必须使用英文完成代码及注释
2. 你必须使用中文来思考和回答