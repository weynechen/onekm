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
sudo ./build/lankm-server /dev/ttyACM0 [baud_rate]

# Baud rate options: 115200, 230400 (default), 460800, 921600
# Higher baud rates reduce latency but require ESP32 configuration

# Examples:
sudo ./build/lankm-server /dev/ttyACM0           # Default 230400 baud
sudo ./build/lankm-server /dev/ttyACM0 460800    # High speed
sudo ./build/lankm-server /dev/ttyACM0 921600    # Maximum speed
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

## Performance Optimization

### Latency Reduction Tips

1. **Increase Baud Rate** (Already implemented)
   - Default: 230400 (was 115200)
   - Maximum: 921600
   - Reduces serial transmission delay from 0.5ms to 0.1ms

2. **Reduce Sleep Delay** (Not implemented)
   - Current: `usleep(100)` = 0.1ms
   - Can reduce to `usleep(10)` or `usleep(0)` (busy wait)
   - Reduces event processing latency

3. **Real-time Scheduling** (Not implemented)
   - Use `SCHED_FIFO` or `SCHED_RR`
   - Requires root privileges
   - Reduces Linux scheduler latency from 1-10ms to <1ms

4. **Increase Mouse Flush Rate** (Implemented)
   - Current: 5ms flush interval
   - Can reduce to 1-2ms for faster mouse response

5. **ESP32 USB HID Rate** (Requires ESP32 firmware change)
   - Current ESP32 rate: ~125Hz (8ms)
   - Can increase to 1000Hz (1ms)
   - Biggest improvement for mouse responsiveness

### Measurement Tools

```bash
# Check USB polling rate on Windows (requires external tool)
# Check serial latency
sudo ./lankm-server /dev/ttyACM0 921600

# Use evtest to see input device latency
evdev-grab /dev/input/eventX
```

## Configuration

- UART port: Configurable via command line argument
- Baud rate: 115200, 230400 (default), 460800, 921600
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