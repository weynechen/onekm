# OneKM - Shared Keyboard & Mouse Controller (Hardware-based)

[中文文档](docs/README.zh-CN.md)

OneKM (Shared Keyboard & Mouse) is a lightweight keyboard and mouse sharing system implemented with a hardware-based solution for cross-platform control.

**Note:** Currently, only Linux is supported as the host.

**Key Advantages:**
- **Linux server** captures input and sends to ESP32 via UART
- **No software installation required** on target computers (Windows/Linux/macOS/any HID-compatible OS)
- Prevents target machine from entering sleep mode
- 100% compatible with all HID-enabled operating systems
- Bypasses all input interception mechanisms
- Ultra-low latency (< 3ms)

## System Architecture

```
[Physical Keyboard/Mouse] → [Linux Server] → [UART] → [ESP32-S3-DevKitC-1] → [USB HID] → [Target Computer (Windows/Linux/macOS/any HID-compatible OS)]
```

### Component Description

1. **Linux Server**: Captures physical input devices and sends commands to ESP32 via UART
2. **ESP32-S3-DevKitC-1**: Receives UART commands and injects input via USB HID ([docs](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/esp32-s3-devkitc-1/index.html))
3. **Target Computer**: Receives USB HID input directly - works with Windows, Linux, macOS, or any USB HID-compatible device (no software required on target)

**Note:** Currently tested on Ubuntu 22.04.

## Hardware Requirements

| Hardware | Description | Purpose |
|----------|-------------|---------|
| **Control Computer** | Linux PC (server runs on Linux only) | Runs onekm-server to capture input |
| ESP32-S3-DevKitC-1 Development Board | With native USB OTG | HID device emulation |
| USB Cable x2 | Micro USB / Type-C | Connect ESP32 to target computer and Linux server |

**Note:** You can replace ESP32-S3-DevKitC-1 with any compatible board.

### ESP32-S3 Pin Connections

```
USB Port ─────────────────────→ Target Computer (plug directly into Windows/Linux/macOS)

USB-to-UART Port ─────────────────────→ Linux server

```
![ESP32-S3-DevKitC-1](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/_images/ESP32-S3-DevKitC-1_v2-annotated-photo.png)

## Build Requirements

### Linux Server
```bash
sudo apt-get install build-essential cmake libevdev-dev libx11-dev
```

### ESP32-S3 (ESP-IDF + TinyUSB)
- ESP-IDF v5.x
- TinyUSB (Espressif official integration)
- ESP32-S3 Dev Module

## Compilation

### Linux Server
```bash
mkdir build && cd build
cmake ..
make
```

### ESP32-S3
```bash
# Set up ESP-IDF environment
source $IDF_PATH/export.sh

cd src/device

# Configure target
idf.py set-target esp32s3

# Build
idf.py build

# Flash (replace with actual port)
idf.py -p /dev/ttyACM0 flash

# Monitor output (optional, for debugging only)
idf.py -p /dev/ttyACM0 monitor
```

## Usage

### 1. Hardware Connection

```
ESP32-S3:
    USB Port ─────────────────────→ Target Computer

    USB-to-UART Port ─────────────→ Linux server
```

Use `ls /dev/tty*` to verify device connection.

### 2. Run Server

```bash
# Requires root privileges to access input devices
sudo ./build/onekm-server /dev/ttyACM0
```

### 3. Operation Instructions

- **Press PAUSE/Break**: Toggle control mode (LOCAL ↔ REMOTE)
    - **REMOTE mode**: All input sent to target computer (Windows/Linux/macOS)
    - **LOCAL mode**: Input affects local Linux system

- **Press PAUSE/Break 3 times within 2 seconds** to exit the program.

## Communication Protocol

### Linux → ESP32 (UART)

**Protocol Type**: Text protocol, each line ends with `\n`

**Message Format**: `TYPE,PARAM1,PARAM2\n`

| Command | Format | Description | Example |
|---------|--------|-------------|---------|
| Mouse Move | `M,dx,dy` | Relative displacement | `M,10,5` |
| Mouse Button | `B,button,state` | Button/state | `B,1,1` (left button press) |
| Keyboard | `K,keycode,state` | Keycode/state | `K,28,1` (Enter press) |
| State Toggle | `S,state` | 1=REMOTE, 0=LOCAL | `S,1` |

**Parameter Description**:
- `dx, dy`: Mouse relative displacement (int16)
- `button`: 1=left, 2=right, 3=middle
- `state`: 1=pressed, 0=released
- `keycode`: Linux evdev keycode (e.g., 28=Enter)

### ESP32 → Windows (USB HID)

**Protocol Type**: Standard USB HID

**Keyboard Report (8 bytes)**:
```
[0] Modifier keys (Ctrl/Shift/Alt/Win)
[1] Reserved
[2-7] 6 keycodes
```

**Mouse Report (4 bytes)**:
```
[0] Button state (bit mask)
[1] X displacement (int8)
[2] Y displacement (int8)
[3] Scroll wheel (int8)
```

## Project Structure

```
onekm/
├── src/
│   ├── common/
│   │   ├── protocol.h          # Message definitions (Message struct)
│   │   └── protocol.c          # Message constructor functions
│   ├── server/                 # Linux Server (C language)
│   │   ├── main.c              # Main program + UART transmission
│   │   ├── input_capture.c     # evdev capture
│   │   ├── input_capture.h
│   │   ├── state_machine.c     # State management (LOCAL/REMOTE)
│   │   └── state_machine.h
│   └── device/                 # ESP32-S3 Firmware (ESP-IDF)
│       ├── main/
│       │   ├── CMakeLists.txt
│       │   ├── idf_component.yml
│       │   ├── onekm_esp32.c   # Main program (UART0 GPIO43/44)
│       │   ├── usb_descriptors.c # USB HID descriptors
│       │   └── uart_parser.c   # UART command parsing
│       ├── CMakeLists.txt
│       ├── sdkconfig.defaults
│       └── README.md
├── docs/
│   ├── design/
│   │   └── design.md           # Design documentation
│   └── README.zh-CN.md         # Chinese documentation
├── CMakeLists.txt              # Linux Server build configuration
├── README.md                   # This file
└── CLAUDE.md                   # Claude Code instructions
```

## Performance Metrics

| Metric | Target | Description |
|--------|--------|-------------|
| End-to-end latency | < 3ms | Linux capture → ESP32 → Target Computer |
| CPU usage | < 1% | Linux Server |
| Memory usage | < 5MB | Linux Server |
| ESP32 processing | < 1ms | UART parsing + HID transmission |

## Advantages Comparison

| Feature | Software Solution | Hardware Solution | Result |
|---------|------------------|-------------------|--------|
| Cross-platform support | Limited/Variable | Universal HID support | Works with any OS |
| Input interception | May be blocked | Always works | Hardware-level injection |
| Security | Requires running code | Pure hardware | Minimal attack surface |
| Latency | 5-10ms (software overhead) | <3ms | Direct hardware HID |
| Installation required | Drivers/software needed | True plug-and-play | No software on target |
| Maintenance | Complex codebase | Simple and reliable | Fewer failure points |

## Troubleshooting

1. **ESP32 not recognized by target computer**: Check USB cable and drivers (works on Windows/Linux/macOS without special drivers)
2. **UART no response**: Check wiring and permissions on Linux server
3. **Input not working**: Confirm in REMOTE mode (toggle with PAUSE/Break) and check target computer is accepting HID input
4. **USB disconnects**: Re-initialize USB connection or try different USB port on target computer

## Core Innovation

Using **ESP32-S3's USB HID capability** as a universal input device completely solves:
- Software-based input interception and blocking
- Cross-platform compatibility issues (works universally on Windows/Linux/macOS)
- Security concerns from running software injectors
- Driver and installation requirements on target systems

## License

MIT License

