# LanKM - 局域网键鼠控制系统（硬件方案）

LanKM （LAN Keyboard & Mouse）是一个轻量级的局域网键鼠共享系统，使用硬件方案实现跨平台控制。

**核心优势：**
- ✅ **Linux服务器**捕获输入并通过UART发送到ESP32
- ✅ 被控端**无需安装任何软件**（Windows/Linux/macOS/任何HID兼容系统通用）
- ✅ 100% 兼容所有支持HID的操作系统
- ✅ 绕过所有输入拦截机制
- ✅ 极低延迟 (< 3ms)
- ✅ **已验证技术栈**: ESP-IDF + TinyUSB

## 系统架构

```
[物理键盘/鼠标] → [Linux服务器] → [UART] → [ESP32-S3-WROOM-1] → [USB] → [被控电脑（Windows/Linux/macOS/任何HID兼容系统）]
```

### 组件说明

1. **Linux Server** (仅Linux): 捕获物理输入，通过UART发送文本命令到ESP32
2. **ESP32-S3-WROOM-1**: 接收UART命令，通过TinyUSB发送HID报文
3. **被控电脑**: 直接接收USB HID输入 - 支持Windows、Linux、macOS或任何USB HID兼容设备（被控端无需软件）

## 硬件需求

| 硬件 | 说明 | 用途 |
|------|------|------|
| **控制电脑** | Linux PC/树莓派（服务器仅支持Linux） | 运行lankm-server捕获输入 |
| ESP32-S3-WROOM-1 开发板 | 带原生 USB OTG | HID 设备模拟 |
| USB 线 | Micro USB / Type-C | 连接被控电脑（任何HID兼容系统） |
| UART 线 | 3根 (TX/RX/GND) | 控制电脑 ↔ ESP32 |

### ESP32-S3 引脚连接
```
USB  ─────────────────────→ 被控电脑（直接插入Windows/Linux/macOS）
GPIO44 (UART0_RX) ←────── Linux UART TX
GPIO43 (UART0_TX) ─────── Linux UART RX
GND                 ────── Linux GND
```

**注意：** UART0 默认用于下载，需重新映射到 GPIO43/44

## 构建要求

### Linux Server
```bash
sudo apt-get install build-essential cmake libevdev-dev libx11-dev
```

### ESP32-S3 (ESP-IDF + TinyUSB)
- ESP-IDF v5.x
- TinyUSB (Espressif 官方集成)
- ESP32-S3 Dev Module

## 编译

### Linux Server
```bash
mkdir build && cd build
cmake ..
make
```

### ESP32-S3
```bash
# 设置 ESP-IDF 环境
source $IDF_PATH/export.sh

cd src/device

# 配置项目
idf.py menuconfig

# 编译
idf.py build

# 烧录 (替换为实际端口)
idf.py -p /dev/ttyACM0 flash

# 监视输出
idf.py -p /dev/ttyACM0 monitor
```

## 使用方法

### 1. 硬件连接

```
ESP32-S3:
    USB  ─────────────→ 被控电脑（Windows/Linux/macOS）
    GPIO16 (RX) ←───── Linux UART TX
    GPIO17 (TX) ────── Linux UART RX
    GND      ────────→ Linux GND
```

### 2. 运行服务器

```bash
# 需要root权限访问输入设备
sudo ./build/lankm-server /dev/ttyACM0
```

### 3. 操作说明

- **F12**: 切换控制模式 (LOCAL ↔ REMOTE)
- **REMOTE模式**: 所有输入发送到被控电脑（Windows/Linux/macOS）
- **LOCAL模式**: 输入影响本地Linux系统

## 权限配置

### Linux udev规则
创建 `/etc/udev/rules.d/99-lankm.rules`:
```
KERNEL=="event*", MODE="0666", GROUP="input"
KERNEL=="ttyUSB*", MODE="0666", GROUP="dialout"
KERNEL=="ttyAMA*", MODE="0666", GROUP="dialout"
```

重新加载规则：
```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## 通信协议

### Linux → ESP32 (UART)

**协议类型**: 文本协议，每行以 `\n` 结尾

**消息格式**: `TYPE,PARAM1,PARAM2\n`

| 命令 | 格式 | 说明 | 示例 |
|------|------|------|------|
| 鼠标移动 | `M,dx,dy` | 相对位移 | `M,10,5` |
| 鼠标按键 | `B,button,state` | 按键/状态 | `B,1,1` (左键按下) |
| 键盘按键 | `K,keycode,state` | 键码/状态 | `K,28,1` (Enter按下) |
| 状态切换 | `S,state` | 1=REMOTE, 0=LOCAL | `S,1` |

**参数说明**:
- `dx, dy`: 鼠标相对位移 (int16)
- `button`: 1=左键, 2=右键, 3=中键
- `state`: 1=按下, 0=释放
- `keycode`: Linux evdev 键码 (如 28=Enter)

### ESP32 → Windows (USB HID)

**协议类型**: 标准 USB HID

**键盘报告 (8字节)**:
```
[0] 修饰键 (Ctrl/Shift/Alt/Win)
[1] 保留
[2-7] 6个按键码
```

**鼠标报告 (4字节)**:
```
[0] 按键状态 (位掩码)
[1] X 位移 (int8)
[2] Y 位移 (int8)
[3] 滚轮 (int8)
```

## 项目结构

```
lankm/
├── src/
│   ├── common/
│   │   ├── protocol.h          # 消息定义 (Message struct)
│   │   └── protocol.c          # 消息构造函数
│   ├── server/                 # Linux 服务器 (C语言)
│   │   ├── main.c              # 主程序 + UART 发送
│   │   ├── input_capture.c     # evdev 捕获
│   │   ├── input_capture.h
│   │   ├── state_machine.c     # 状态管理 (LOCAL/REMOTE)
│   │   └── state_machine.h
│   └── device/                 # ESP32-S3 固件 (ESP-IDF)
│       ├── main/
│       │   ├── CMakeLists.txt
│       │   ├── idf_component.yml
│       │   ├── lankm_esp32.c    # 主程序（UART0 GPIO43/44）
│       │   ├── usb_descriptors.c # USB HID 描述符
│       │   └── uart_parser.c    # UART 命令解析
│       ├── CMakeLists.txt
│       ├── sdkconfig.defaults
│       └── README.md
├── docs/
│   └── design/
│       └── design.md           # 设计文档
├── CMakeLists.txt              # Linux 服务器构建
├── README.md
└── CLAUDE.md
```

## 性能指标

| 指标 | 目标 | 说明 |
|------|------|------|
| 端到端延迟 | < 3ms | Linux捕获 → ESP32 → 被控电脑 |
| CPU 占用 | < 1% | Linux 服务器 |
| 内存占用 | < 5MB | Linux 服务器 |
| ESP32 处理 | < 1ms | UART解析 + HID发送 |

## 优势对比

| 特性 | 软件方案 | 硬件方案 (本项目) |
|------|---------|------------------|
| 跨平台支持 | 有限/不确定 | ✅ 通用HID支持 |
| 输入拦截 | 可能被拦截 | ✅ 始终有效 |
| 安全性 | 需要运行代码 | ✅ 纯硬件 |
| 延迟 | 5-10ms | ✅ <3ms |
| 无需安装 | ❌ | ✅ |
| 维护成本 | 高 | ✅ 低 |

## 故障排除

1. **ESP32未被被控电脑识别**: 检查USB线和驱动（在Windows/Linux/macOS上均无需特殊驱动）
2. **UART无响应**: 检查接线和Linux服务器权限
3. **输入无效果**: 确认处于REMOTE模式 (F12切换) 并检查被控电脑是否接受HID输入
4. **USB断开**: 重新初始化USB连接或更换被控电脑的USB端口

## 核心创新

使用 **ESP32-S3 的 USB HID 功能** 作为通用输入设备，彻底解决了：
- 软件输入拦截和阻止问题
- 跨平台兼容性问题（在Windows/Linux/macOS上通用）
- 软件注入器的安全风险
- 被控系统上的驱动和安装要求

## 许可证

MIT License
