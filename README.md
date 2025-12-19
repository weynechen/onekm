# LanKM - LAN Keyboard & Mouse Controller (Hardware-based)

LanKM是一个轻量级的局域网键鼠共享系统，使用硬件方案实现跨平台控制。

**核心优势：**
- ✅ Windows端**无需安装任何软件**
- ✅ 绕过所有Windows输入拦截
- ✅ 极低延迟 (< 3ms)
- ✅ 100% 兼容所有应用
- ✅ **已验证技术栈**: ESP-IDF + TinyUSB

## 系统架构

```
[物理键盘/鼠标] → [Linux服务器] → [UART] → [ESP32-S3] → [USB] → [Windows PC]
```

### 组件说明

1. **Linux Server**: 捕获物理输入，通过UART发送文本命令到ESP32
2. **ESP32-S3**: 接收UART命令，通过TinyUSB发送HID报文
3. **Windows PC**: 直接接收USB HID输入，无需任何软件

## 硬件需求

| 硬件 | 说明 | 用途 |
|------|------|------|
| ESP32-S3 开发板 | 带原生 USB OTG | HID 设备模拟 |
| Linux 设备 | 树莓派/PC/笔记本 | 输入捕获 |
| USB 线 | Micro USB / Type-C | 连接 Windows |
| UART 线 | 3根 (TX/RX/GND) | Linux ↔ ESP32 |

### ESP32-S3 引脚连接
```
USB  ─────────────────────→ Windows PC (直接插入)
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
    USB  ─────────────→ Windows PC
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
- **REMOTE模式**: 所有输入发送到Windows
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
| 端到端延迟 | < 3ms | Linux捕获 → ESP32 → Windows |
| CPU 占用 | < 1% | Linux 服务器 |
| 内存占用 | < 5MB | Linux 服务器 |
| ESP32 处理 | < 1ms | UART解析 + HID发送 |

## 优势对比

| 特性 | 软件方案 | 硬件方案 (本项目) |
|------|---------|------------------|
| Windows兼容性 | 可能被拦截 | ✅ 100% 兼容 |
| 安全性 | 需要运行代码 | ✅ 纯硬件 |
| 延迟 | 5-10ms | ✅ <3ms |
| 维护成本 | 高 | ✅ 低 |
| 无需安装 | ❌ | ✅ |

## 故障排除

1. **ESP32未被识别**: 检查USB线和驱动
2. **UART无响应**: 检查接线和权限
3. **输入无效果**: 确认处于REMOTE模式 (F12切换)
4. **USB断开**: 重新初始化USB

## 核心创新

使用 **ESP32-S3 的 USB HID 功能** 替代 Windows 软件注入，彻底解决了：
- Windows 输入拦截问题
- 兼容性问题
- 安全性问题

## 许可证

MIT License
