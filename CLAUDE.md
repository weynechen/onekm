# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

LanKM is a lightweight LAN keyboard and mouse sharing system that allows controlling multiple computers with a single keyboard and mouse.

**Architecture:**
- **Control Server (Linux only)**: Captures physical input devices and sends to ESP32 via UART
- **ESP32-S3-WROOM-1**: Receives commands via UART and injects input via USB HID
- **Target Computer (Windows/Linux/macOS/any HID-compatible)**: Receives USB HID input from ESP32 (no software required)

## Build Commands

```bash
# Build Server
mkdir build && cd build
cmake ..
make

# Install
sudo make install
```

## Dependencies

### Linux Server Dependencies:
```bash
sudo apt-get install build-essential cmake libevdev-dev libx11-dev
```

## Running

```bash
# Linux Server (requires root)
sudo ./build/lankm-server /dev/ttyACM0
```

## Architecture

### Core Components:

1. **Protocol Layer** (`src/common/`):
   - Simple binary protocol for input events
   - Message struct: `{uint8_t type, int16_t a, int16_t b}`
   - Message types: mouse move, mouse button, keyboard events, control switch

2. **Server (Linux)**:
   - `input_capture.c`: Captures input using libevdev
   - `state_machine.c`: Manages control state (local/remote)
   - `main.c`: Sends commands to ESP32 via UART

3. **ESP32-S3** (separate firmware):
   - Receives UART commands
   - Injects input via USB HID gadget

### Communication Architecture:
- **Linux → ESP32**: UART (115200 baud, 8N1)
- **ESP32 → Windows**: USB HID (keyboard + mouse)
- Unidirectional flow: Server → ESP32 → Windows
- No network connection needed to Windows

### Key Design Decisions:
- Hardware-based injection bypasses Windows software restrictions
- Minimal latency (<3ms) via direct USB HID
- Binary protocol for efficiency
- No authentication needed (trusted LAN + hardware)

## Development Notes

- Uses C11 standard
- Compiler warnings enabled (-Wall -Wextra -Werror)
- Struct packing for protocol compatibility
- No unit tests currently implemented
- Uses clang-format for code formatting (if available)

## Configuration

- UART port: Configurable via command line argument
- Default baud rate: 115200
- No configuration file - all settings are compile-time constants

## Permissions

Linux server requires access to input devices and UART:
```
KERNEL=="event*", MODE="0666", GROUP="input"
KERNEL=="ttyUSB*", MODE="0666", GROUP="dialout"
```

## Additional Documentation

- [README.md](README.md) - Main project documentation (English)
- [中文文档](docs/README.zh-CN.md) - Chinese documentation
- [设计文档](docs/design/design.md) - Design documentation (Chinese)

## Rules
1. Code and comments must be written in English
2. Think and respond in Chinese when communicating with users
3. For ESP32 testing: load `idf-master` environment variables once, then run `idf.py build app-flash monitor -p /dev/ttyACM0` 