# OneKM - 键盘鼠标控制器（硬件方案）

[English Documentation](README.md)

OneKM（Shared Keyboard & Mouse）是一个轻量级的局域网键盘鼠标共享系统，采用硬件方案实现跨平台控制。

**注意：** 目前仅支持 Linux 作为主机端。

**主要优势：**
- **Linux 服务器**捕获输入设备并通过 UART 发送给 ESP32
- **目标计算机无需安装任何软件**（支持 Windows/Linux/macOS/任何 HID 兼容系统）
- 防止目标计算机进入休眠状态
- 100% 兼容所有支持 HID 的操作系统
- 绕过所有输入拦截机制
- 超低延迟（< 3ms）

## 系统架构

```
[物理键盘/鼠标] → [Linux 服务器] → [UART] → [ESP32-S3-DevKitC-1] → [USB HID] → [目标计算机 (Windows/Linux/macOS/任何 HID 兼容系统)]
```

### 组件说明

1. **Linux 服务器**：捕获物理输入设备并通过 UART 向 ESP32 发送命令
2. **ESP32-S3-DevKitC-1**：接收 UART 命令并通过 USB HID 注入输入（[文档](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/esp32-s3-devkitc-1/index.html)）
3. **目标计算机**：直接接收 USB HID 输入 - 支持 Windows、Linux、macOS 或任何 USB HID 兼容设备（目标端无需安装软件）

**注意：** 目前在 Ubuntu 22.04 上测试通过。

## 硬件要求

| 硬件 | 描述 | 用途 |
|------|------|------|
| **控制计算机** | Linux 电脑（服务器仅支持 Linux） | 运行 onekm-server 捕获输入 |
| ESP32-S3-DevKitC-1 开发板 | 带有原生 USB OTG | HID 设备模拟 |
| USB 数据线 x2 | Micro USB / Type-C | 连接 ESP32 到目标计算机和 Linux 服务器 |

**注意：** 可以使用任何兼容的开发板替代 ESP32-S3-DevKitC-1。

### ESP32-S3 引脚连接

```
USB 接口 ─────────────────────→ 目标计算机（直接插入 Windows/Linux/macOS）

USB 转 UART 接口 ─────────────────────→ Linux 服务器

```
![ESP32-S3-DevKitC-1](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/_images/ESP32-S3-DevKitC-1_v2-annotated-photo.png)

## 构建要求

### Linux 服务器
```bash
sudo apt-get install build-essential cmake libevdev-dev libx11-dev
```

### ESP32-S3（ESP-IDF + TinyUSB）
- ESP-IDF v5.x
- TinyUSB（Espressif 官方集成）
- ESP32-S3 Dev Module

## 编译

### Linux 服务器
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

# 配置目标芯片
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录（替换为实际端口）
idf.py -p /dev/ttyACM0 flash

# 监控输出（可选，仅用于调试）
idf.py -p /dev/ttyACM0 monitor
```

## 使用方法

### 1. 硬件连接

```
ESP32-S3:
    USB 接口 ─────────────────────→ 目标计算机

    USB 转 UART 接口 ─────────────→ Linux 服务器
```

使用 `ls /dev/tty*` 命令验证设备连接。

### 2. 运行服务器

```bash
# 需要 root 权限来访问输入设备
sudo ./build/onekm-server /dev/ttyACM0
```

### 3. 操作说明

- **按下 PAUSE/Break 键**：切换控制模式（本地 ↔ 远程）
    - **远程模式**：所有输入发送到目标计算机（Windows/Linux/macOS）
    - **本地模式**：输入仅影响本地 Linux 系统

- **2 秒内按下 PAUSE/Break 键 3 次**：退出程序

## 通信协议

### Linux → ESP32（UART）

**协议类型**：文本协议，每行以 `\n` 结尾

**消息格式**：`TYPE,PARAM1,PARAM2\n`

| 命令 | 格式 | 描述 | 示例 |
|------|------|------|------|
| 鼠标移动 | `M,dx,dy` | 相对位移 | `M,10,5` |
| 鼠标按键 | `B,button,state` | 按键/状态 | `B,1,1`（左键按下） |
| 键盘 | `K,keycode,state` | 按键码/状态 | `K,28,1`（回车按下） |
| 状态切换 | `S,state` | 1=远程, 0=本地 | `S,1` |

**参数说明**：
- `dx, dy`：鼠标相对位移（int16）
- `button`：1=左键，2=右键，3=中键
- `state`：1=按下，0=释放
- `keycode`：Linux evdev 按键码（例如：28=回车）

### ESP32 → Windows（USB HID）

**协议类型**：标准 USB HID

**键盘报告（8 字节）**：
```
[0] 修饰键（Ctrl/Shift/Alt/Win）
[1] 保留
[2-7] 6 个按键码
```

**鼠标报告（4 字节）**：
```
[0] 按键状态（位掩码）
[1] X 位移（int8）
[2] Y 位移（int8）
[3] 滚轮（int8）
```

## 项目结构

```
onekm/
├── src/
│   ├── common/
│   │   ├── protocol.h          # 消息定义（Message 结构体）
│   │   └── protocol.c          # 消息构造函数
│   ├── server/                 # Linux 服务器（C 语言）
│   │   ├── main.c              # 主程序 + UART 传输
│   │   ├── input_capture.c     # evdev 捕获
│   │   ├── input_capture.h
│   │   ├── state_machine.c     # 状态管理（本地/远程）
│   │   └── state_machine.h
│   └── device/                 # ESP32-S3 固件（ESP-IDF）
│       ├── main/
│       │   ├── CMakeLists.txt
│       │   ├── idf_component.yml
│       │   ├── onekm_esp32.c   # 主程序（UART0 GPIO43/44）
│       │   ├── usb_descriptors.c # USB HID 描述符
│       │   └── uart_parser.c   # UART 命令解析
│       ├── CMakeLists.txt
│       ├── sdkconfig.defaults
│       └── README.md
├── docs/
│   ├── design/
│   │   └── design.md           # 设计文档
│   └── README.zh-CN.md         # 中文文档
├── CMakeLists.txt              # Linux 服务器构建配置
├── README.md                   # 英文文档
└── CLAUDE.md                   # Claude Code 说明
```

## 性能指标

| 指标 | 目标值 | 描述 |
|------|--------|------|
| 端到端延迟 | < 3ms | Linux 捕获 → ESP32 → 目标计算机 |
| CPU 占用 | < 1% | Linux 服务器 |
| 内存占用 | < 5MB | Linux 服务器 |
| ESP32 处理 | < 1ms | UART 解析 + HID 传输 |

## 方案对比

| 特性 | 软件方案 | 硬件方案 | 结果 |
|------|----------|----------|------|
| 跨平台支持 | 有限/不稳定 | 通用 HID 支持 | 兼容任何系统 |
| 输入拦截 | 可能被拦截 | 始终有效 | 硬件级注入 |
| 安全性 | 需要运行代码 | 纯硬件 | 最小攻击面 |
| 延迟 | 5-10ms（软件开销） | <3ms | 直接硬件 HID |
| 需安装软件 | 需要驱动/软件 | 即插即用 | 目标端无需软件 |
| 维护 | 复杂的代码库 | 简单可靠 | 更少的故障点 |

## 故障排除

1. **ESP32 未被目标计算机识别**：检查 USB 数据线和驱动程序（在 Windows/Linux/macOS 上无需特殊驱动）
2. **UART 无响应**：检查 Linux 服务器上的接线和权限
3. **输入无反应**：确认处于远程模式（使用 PAUSE/Break 切换）并检查目标计算机是否接受 HID 输入
4. **USB 断开连接**：重新初始化 USB 连接或尝试目标计算机上的其他 USB 端口

## 核心创新

使用 **ESP32-S3 的 USB HID 功能**作为通用输入设备，完全解决了以下问题：
- 基于软件的输入拦截和阻断
- 跨平台兼容性问题（在 Windows/Linux/macOS 上普遍适用）
- 运行软件注入器的安全顾虑
- 目标系统上的驱动和安装要求

## 许可证

MIT License

