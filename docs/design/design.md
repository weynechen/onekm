# OneKM 设计文档 - 硬件方案 (ESP-IDF + TinyUSB)

## 1. 项目概述

**OneKM** (Shared Keyboard & Mouse) 是一个基于硬件的局域网键盘鼠标共享系统。

### 1.1 核心优势

- **目标计算机端无需任何软件**：使用 USB HID 硬件模拟
- **绕过所有输入拦截**：操作系统识别为真实硬件设备
- **极低延迟**：端到端延迟 < 3ms
- **100% 兼容性**：支持所有 HID 兼容操作系统
- **防止目标计算机进入休眠状态**

### 1.2 系统架构

```
[物理键盘/鼠标] → [Linux 服务器] → [UART] → [ESP32-S3-DevKitC-1] → [USB HID] → [目标计算机 (Windows/Linux/macOS)]
```

**组件角色：**
- **Linux Server**: 捕获物理输入设备，通过 UART 发送文本命令
- **ESP32-S3-DevKitC-1**: 接收 UART 命令，通过 TinyUSB 发送 HID 报文
- **目标计算机**: 直接接收 USB HID 输入，无需任何软件

**注意：** 目前在 Ubuntu 22.04 上测试通过。

---

## 2. 硬件设计

### 2.1 所需硬件

| 硬件 | 描述 | 用途 |
|------|------|------|
| ESP32-S3-DevKitC-1 开发板 | 带有原生 USB OTG | HID 设备模拟 |
| Linux 设备 | Linux 电脑（服务器仅支持 Linux） | 输入捕获 |
| USB 数据线 x2 | Micro USB / Type-C | 连接目标计算机和 Linux 服务器 |

**注意：** 可以使用任何兼容的开发板替代 ESP32-S3-DevKitC-1。

### 2.2 硬件连接

**ESP32-S3-DevKitC-1 连接方式：**

```
USB 接口 ─────────────────────→ 目标计算机（直接插入 Windows/Linux/macOS）

USB 转 UART 接口 ─────────────────────→ Linux 服务器
```

![ESP32-S3-DevKitC-1](https://docs.espressif.com/projects/esp-dev-kits/zh_CN/latest/esp32s3/_images/ESP32-S3-DevKitC-1_v2-annotated-photo.png)

**注意：**
- USB 接口用于连接目标计算机，注入 HID 输入
- USB 转 UART 接口用于连接 Linux 服务器，接收命令
- 供电：通过 USB 接口供电

---

## 3. 软件架构

### 3.1 Linux Server (C语言)

#### 3.1.1 模块划分

```
src/
├── common/
│   ├── protocol.h          # 协议定义 (Message struct)
│   └── protocol.c          # 消息构造函数
├── server/
│   ├── main.c              # 主程序 + UART 通信
│   ├── input_capture.c     # evdev 输入捕获
│   ├── input_capture.h
│   ├── state_machine.c     # 状态管理 (LOCAL/REMOTE)
│   └── state_machine.h
├── keyboard_state.c        # 键盘状态管理
└── keyboard_state.h
```

#### 3.1.2 主程序流程

```
初始化
   ↓
打开 evdev 设备
   ↓
打开 UART 设备
   ↓
初始化状态机 (LOCAL)
   ↓
主循环
   ├─ 捕获输入事件
   ├─ 状态机处理
   ├─ 生成 UART 命令
   └─ 发送到 ESP32
```

#### 3.1.3 输入捕获 (input_capture.c)

**技术**: Linux evdev

**捕获内容：**
- 鼠标相对移动：`REL_X`, `REL_Y`
- 鼠标按键：`BTN_LEFT`, `BTN_RIGHT`, `BTN_MIDDLE`
- 键盘按键：`KEY_*`
- 特殊按键：`KEY_PAUSE`（模式切换）

**实现要点：**
```c
// 打开输入设备
int fd = open("/dev/input/eventX", O_RDONLY | O_NONBLOCK);

// 读取事件
struct input_event ev;
while (read(fd, &ev, sizeof(ev)) > 0) {
    // 处理事件
}
```

#### 3.1.4 状态机 (state_machine.c)

**状态定义：**
```c
typedef enum {
    STATE_LOCAL,   // 本地控制
    STATE_REMOTE   // 远程控制
} ControlState;
```

**状态转换：**
- **触发条件**: PAUSE/Break 按键
- **LOCAL → REMOTE**: 抓取输入设备，发送切换命令
- **REMOTE → LOCAL**: 释放输入设备，发送切换命令
- **快速按 3 次 PAUSE/Break（2 秒内）**: 退出程序

**异常处理：**
- UART 断开 → 自动回到 LOCAL，强制释放所有按键
- 网络断开 → 自动回到 LOCAL

#### 3.1.5 UART 通信 (main.c)

**配置：**
- 波特率: 230400（默认，可选 115200/460800/921600）
- 数据位: 8
- 停止位: 1
- 校验: None

**协议格式：**
```
文本协议，每行以 \n 结尾
格式: TYPE,PARAM1,PARAM2\n
```

---

### 3.2 ESP32-S3 固件 (ESP-IDF + TinyUSB)

**技术栈：**
- **框架**: ESP-IDF v5.x
- **USB库**: TinyUSB（Espressif 官方集成）
- **开发板**: ESP32-S3 Dev Module

#### 3.2.1 任务架构

```
┌─────────────────────────────┐
│ UART 接收任务 (Core 0)       │ ← 从 Linux 接收命令
│  - 读取串口数据              │
│  - 解析文本协议              │
│  - 更新共享状态              │
└──────────────┬──────────────┘
               │ (队列/信号量)
┌──────────────▼──────────────┐
│ HID 发送任务 (Core 1)        │ → 向目标计算机发送 HID
│  - 监听状态变化              │
│  - 调用 TinyUSB API          │
└─────────────────────────────┘
```

#### 3.2.2 UART 接收任务实现

**配置：**
```c
// UART0: GPIO44(RX), GPIO43(TX)
const uart_config_t uart_config = {
    .baud_rate = 230400,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
};
uart_driver_install(UART_NUM_0, 256, 0, 0, NULL);

// 映射引脚
esp_rom_gpio_connect_out_signal(GPIO_NUM_43, UART_PERIPH_SIGNAL(0, SOC_UART_TX_PIN_IDX), false, false);
esp_rom_gpio_connect_in_signal(GPIO_NUM_44, UART_PERIPH_SIGNAL(0, SOC_UART_RX_PIN_IDX), false, false);
```

**接收循环：**
```c
void uart_task(void *pvParameters) {
    uint8_t data[128];
    while (1) {
        int len = uart_read_bytes(UART_NUM_0, data, sizeof(data), 10 / portTICK_PERIOD_MS);
        if (len > 0) {
            // 解析命令并更新状态
            parse_and_update(data, len);
        }
    }
}
```

#### 3.2.3 HID 发送任务实现

**TinyUSB 配置：**
```c
// usb_descriptors.c
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE))
};
```

**HID 发送：**
```c
void hid_task(void *pvParameters) {
    while (1) {
        if (xSemaphoreTake(hid_update_sem, portMAX_DELAY) == pdTRUE) {
            // 发送键盘事件
            if (keyboard_state.changed) {
                tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD,
                    keyboard_state.modifiers, keyboard_state.keys);
            }

            // 发送鼠标事件
            if (mouse_state.changed) {
                tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE,
                    mouse_state.buttons, mouse_state.x, mouse_state.y, 0, 0);
            }
        }
    }
}
```

**关键特性：**
- 使用 **信号量** 同步两个任务
- **共享状态结构体** 保存当前输入状态
- **按需发送**：只有状态变化时才调用 TinyUSB API

#### 3.2.4 共享数据结构

```c
// 鼠标状态
typedef struct {
    int8_t x;              // X 位移
    int8_t y;              // Y 位移
    uint8_t buttons;       // 按键位掩码 (bit0=左, bit1=右, bit2=中)
    bool changed;          // 状态变化标志
} mouse_state_t;

// 键盘状态
typedef struct {
    uint8_t modifiers;     // 修饰键 (Ctrl/Shift/Alt/Win)
    uint8_t keys[6];       // 6个按键码
    bool changed;          // 状态变化标志
} keyboard_state_t;

// 全局共享变量（需互斥锁保护）
static mouse_state_t mouse_state = {0};
static keyboard_state_t keyboard_state = {0};
static SemaphoreHandle_t state_mutex;      // 保护共享状态
static SemaphoreHandle_t hid_update_sem;   // 触发 HID 发送
```

---

## 4. 通信协议设计

### 4.1 Linux → ESP32 (UART)

**协议类型**: 文本协议

**消息格式：**
```
TYPE,PARAM1,PARAM2\n
```

**消息类型：**

| 命令 | 格式 | 描述 | 示例 |
|------|------|------|------|
| 鼠标移动 | `M,dx,dy` | 相对位移 | `M,10,5` |
| 鼠标按键 | `B,button,state` | 按键/状态 | `B,1,1`（左键按下） |
| 键盘按键 | `K,keycode,state` | 按键码/状态 | `K,28,1`（回车按下） |
| 状态切换 | `S,state` | 1=远程, 0=本地 | `S,1` |

**参数说明：**
- `dx, dy`：鼠标相对位移（int16）
- `button`：1=左键，2=右键，3=中键
- `state`：1=按下，0=释放
- `keycode`：Linux evdev 按键码（例如：28=回车）

### 4.2 ESP32 → 目标计算机 (USB HID)

**协议类型**: 标准 USB HID

**键盘报告（8 字节）：**
```
[0] 修饰键（Ctrl/Shift/Alt/Win）
[1] 保留
[2-7] 6 个按键码
```

**鼠标报告（4 字节）：**
```
[0] 按键状态（位掩码）
[1] X 位移（int8）
[2] Y 位移（int8）
[3] 滚轮（int8）
```

---

## 5. 关键实现细节

### 5.1 Linux 服务器

#### 5.1.1 输入设备抓取

```c
// 需要 root 权限
int fd = open("/dev/input/eventX", O_RDONLY);

// 独占访问 (防止事件影响本地)
ioctl(fd, EVIOCGRAB, 1);
```

#### 5.1.2 虚拟光标维护

```c
static int cursor_x = 0, cursor_y = 0;

void process_mouse_move(int dx, int dy) {
    cursor_x += dx;
    cursor_y += dy;

    // 边界检测
    if (cursor_x <= 0) {
        switch_to_remote();
    }
}
```

#### 5.1.3 UART 发送

```c
void send_to_esp32(const char *fmt, ...) {
    char buf[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // 添加换行符
    strcat(buf, "\n");

    write(uart_fd, buf, strlen(buf));
}
```

### 5.2 ESP32-S3 固件

#### 5.2.1 USB HID 初始化

```c
#include "tusb.h"

void hid_init(void) {
    tusb_init();
}

void app_main(void) {
    hid_init();
    // 创建 UART 接收任务
    // 创建 HID 发送任务
}
```

#### 5.2.2 UART 接收

```c
void loop(void) {
    while (uart_read_bytes(UART_NUM_0, buffer, len, 10 / portTICK_PERIOD_MS) > 0) {
        if (buffer contains '\n') {
            process_command(line);
            clear_buffer();
        }
    }
}
```

#### 5.2.3 ESP32 端命令解析

```c
// 解析 "M,10,5\n" 格式的命令
static void parse_command(const char *line) {
    char type;
    int param1, param2;

    // 解析命令类型
    if (sscanf(line, "%c,%d,%d", &type, ¶m1, ¶m2) != 3) {
        return; // 格式错误
    }

    switch (type) {
        case 'M': // 鼠标移动
            xSemaphoreTake(state_mutex, portMAX_DELAY);
            mouse_state.x = param1;
            mouse_state.y = param2;
            mouse_state.changed = true;
            xSemaphoreGive(state_mutex);
            xSemaphoreGive(hid_update_sem); // 触发 HID 发送
            break;

        case 'B': // 鼠标按键
            xSemaphoreTake(state_mutex, portMAX_DELAY);
            if (param2) {
                mouse_state.buttons |= (1 << (param1 - 1));
            } else {
                mouse_state.buttons &= ~(1 << (param1 - 1));
            }
            mouse_state.changed = true;
            xSemaphoreGive(state_mutex);
            xSemaphoreGive(hid_update_sem);
            break;

        case 'K': // 键盘按键
            xSemaphoreTake(state_mutex, portMAX_DELAY);
            if (param2) {
                // 按下：添加到按键数组
                keyboard_state.keys[0] = param1;
            } else {
                // 释放：清空按键数组
                memset(keyboard_state.keys, 0, 6);
            }
            keyboard_state.changed = true;
            xSemaphoreGive(state_mutex);
            xSemaphoreGive(hid_update_sem);
            break;

        case 'S': // 状态切换（可选：LED 指示）
            if (param1 == 1) {
                gpio_set_level(LED_PIN, 1); // REMOTE: LED 亮
            } else {
                gpio_set_level(LED_PIN, 0); // LOCAL: LED 灭
            }
            break;
    }
}
```

---

## 6. 项目结构

### 6.1 目录结构

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
│   │   ├── state_machine.h
│   │   ├── keyboard_state.c    # 键盘状态管理
│   │   └── keyboard_state.h
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
│   │   └── design.md           # 本设计文档
│   └── README.zh-CN.md         # 中文文档
├── CMakeLists.txt              # Linux 服务器构建配置
├── README.md                   # 英文文档
└── CLAUDE.md                   # Claude Code 说明
```

### 6.2 ESP32-S3 源文件说明

**主要文件：** `src/device/main/onekm_esp32.c`

主要函数：
- `app_main()` - 初始化和任务创建
- `uart_receive_task()` - UART 接收任务
- `hid_send_task()` - HID 发送任务
- `parse_command()` - 命令解析
- 共享状态变量：`mouse_state`, `keyboard_state`

**主要文件：** `src/device/main/usb_descriptors.c`

TinyUSB 描述符配置：
- 设备描述符
- 配置描述符
- HID 报告描述符（键盘 + 鼠标）
- 字符串描述符

**主要文件：** `src/device/main/uart_parser.c`

UART 命令解析：
- 行缓冲处理
- 协议解析 (M/B/K/S)
- 状态更新与信号量触发

### 6.3 运行权限

**测试阶段（推荐）：**
```bash
# 直接使用 sudo 运行（最简单，无需配置）
sudo ./build/onekm-server /dev/ttyACM0
```

**说明：**
- 测试阶段使用 `sudo` 即可，无需额外配置
- 使用 `ls /dev/tty*` 验证设备连接

---

## 7. 性能指标

| 指标 | 目标值 | 描述 |
|------|--------|------|
| 端到端延迟 | < 3ms | Linux 捕获 → ESP32 → 目标计算机 |
| CPU 占用 | < 1% | Linux 服务器 |
| 内存占用 | < 5MB | Linux 服务器 |
| ESP32 处理 | < 1ms | UART 解析 + HID 传输 |

---

## 8. 异常处理

### 8.1 Linux 服务器

| 场景 | 行为 |
|------|------|
| UART 断开 | 停止捕获，提示错误 |
| 设备无响应 | 自动回到 LOCAL 模式，强制释放所有按键 |
| 按键卡住 | 状态切换时强制释放 |

### 8.2 ESP32-S3

| 场景 | 行为 |
|------|------|
| USB 断开 | 重新初始化 USB |
| UART 数据错误 | 忽略错误行，继续接收 |
| 看门狗超时 | 自动重启 |

---

## 9. 开发与构建

### 9.1 Linux 服务器

```bash
# 依赖安装
sudo apt-get install build-essential cmake libevdev-dev libx11-dev

# 编译
mkdir build && cd build
cmake ..
make

# 运行
sudo ./onekm-server /dev/ttyACM0
```

### 9.2 ESP32-S3 (ESP-IDF)

**环境准备：**
```bash
# 安装 ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32s3
source export.sh

# 或者使用已安装的 ESP-IDF
source $IDF_PATH/export.sh
```

**编译与烧录：**
```bash
cd src/device

# 配置目标芯片
idf.py set-target esp32s3

# 编译
idf.py build

# 烧录到 ESP32-S3（替换为实际端口）
idf.py -p /dev/ttyACM0 flash

# 监控输出（可选，用于调试）
idf.py -p /dev/ttyACM0 monitor
```

**关键配置：**
- Board: ESP32-S3 Dev Module
- USB CDC On Boot: Enabled
- USB OTG: Enabled

---

## 10. 测试方案

### 10.1 单元测试

- 协议解析测试
- 状态机转换测试
- UART 命令生成测试

### 10.2 集成测试

1. **UART 环回测试**
   - Linux 发送命令到 ESP32
   - ESP32 回显验证

2. **HID 功能测试**
   - ESP32 连接目标计算机
   - 验证键盘/鼠标识别

3. **端到端测试**
   - 完整链路验证
   - 延迟测量

---

## 11. 扩展性考虑

### 11.1 可能的扩展

- **多设备支持**: 多个 ESP32 连接不同目标计算机
- **配置界面**: Web UI 配置参数
- **状态指示**: LED 指示当前模式
- **日志系统**: 详细日志记录

### 11.2 当前限制

- 单向通信（Linux → 目标计算机）
- 不支持剪贴板共享
- 不支持文件传输

---

## 12. 总结

### 12.1 核心创新

使用 **ESP32-S3 的 USB HID 功能** 替代软件注入，彻底解决了：
- 操作系统输入拦截问题
- 跨平台兼容性问题（在 Windows/Linux/macOS 上通用）
- 安全性问题（无需运行注入软件）

### 12.2 实施要点

1. **Linux 端**: 保持现有 evdev 捕获，修改发送目标为 UART
2. **ESP32 端**: 实现 UART 接收 + USB HID 注入
3. **目标计算机端**: 零改动，即插即用

### 12.3 预期效果

- 目标计算机端无需安装任何软件
- 绕过所有安全软件的拦截
- 支持所有操作系统和应用
- 极低延迟，体验流畅
