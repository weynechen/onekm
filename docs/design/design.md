# 设计文档

## 项目名称

**LanKM** —— LAN Keyboard & Mouse Controller

---

## 1. 设计目标（Design Goals）

### 1.1 功能目标

* 支持 **两台局域网内机器**

  * Ubuntu Linux（Server，物理键鼠）
  * Windows（Client，被控）
* 一套键盘 + 鼠标控制两台机器
* 鼠标移动到屏幕边缘时自动切换控制权
* 无 UI、无配置文件（或极简配置）
* 延迟低、行为确定、可预测

### 1.2 非目标（Out of Scope）

* ❌ 不支持多台机器
* ❌ 不支持动态拓扑
* ❌ 不支持 GUI
* ❌ 不考虑 Wayland（仅 X11 / tty / evdev）

### 1.3 后续扩展
*  复杂剪贴板 / 文件传输

---

## 2. 系统架构概览

### 2.1 总体架构

```
┌────────────────────┐        TCP        ┌────────────────────┐
│ Ubuntu (Server)    │ ───────────────▶ │ Windows (Client)   │
│                    │                  │                    │
│  evdev 捕获        │                  │  SendInput 注入    │
│  状态机 / 边缘判断 │                  │  虚拟输入设备      │
└────────────────────┘                  └────────────────────┘
```
两者角色可以互换。


### 2.2 角色定义

| 角色     | 说明               |
| ------ | ---------------- |
| Server | 连接真实键盘鼠标，负责捕获与决策 |
| Client | 仅接收输入并注入系统       |

---

## 3. 核心原理

### 3.1 本质模型

> **输入设备虚拟化 + 网络转发**

```
物理输入 → 捕获 → 序列化 → 网络 → 反序列化 → 系统注入
```

---

## 4. 模块划分
以ubuntu为server，window作为client为例

### 4.1 Server（Ubuntu）

#### 4.1.1 Input Capture 模块

* 技术：**Linux evdev**
* 设备：

  * `/dev/input/eventX`
* 捕获内容：

  * 鼠标相对移动（REL_X / REL_Y）
  * 鼠标按键（BTN_LEFT / BTN_RIGHT）
  * 键盘按键（KEY_*）

> 直接读取 `struct input_event`

---

#### 4.1.2 Virtual Cursor & Edge Detection

* 维护逻辑鼠标坐标：

```c
int cursor_x;
int cursor_y;
```

* 屏幕信息来源：

  * X11：`XDisplayWidth / XDisplayHeight`
  * 或 DRM / 固定配置

* 边缘判定规则（示例）：

```text
cursor_x <= 0            → 切换到 Client
cursor_x >= screen_w    → 切回 Server
```

---

#### 4.1.3 Control State Machine

```c
enum ControlState {
    LOCAL,
    REMOTE
};
```

| 当前状态   | 事件     | 下一个状态  |
| ------ | ------ | ------ |
| LOCAL  | 鼠标越界   | REMOTE |
| REMOTE | 鼠标反向越界 | LOCAL  |

状态切换动作：

* 清空按键状态
* 同步鼠标位置
* 切换事件转发目标

---

#### 4.1.4 Network 模块

* 传输协议：**TCP**
* 端口：固定（如 24800）
* Server 主动监听
* Client 主动连接

---

### 4.2 Client（Windows）

#### 4.2.1 Network Receiver

* TCP 客户端
* 阻塞读取消息
* 反序列化输入事件

---

#### 4.2.2 Input Injection

* API：**Win32 SendInput**
* 支持：

  * 鼠标移动
  * 鼠标按键
  * 键盘按键

> 不需要管理员权限

---

## 5. 通信协议设计（极简）

### 5.1 消息格式（二进制，定长）

```c
struct Msg {
    uint8_t type;
    int16_t a;
    int16_t b;
};
```

### 5.2 消息类型定义

| type | 含义           | a       | b     |
| ---- | ------------ | ------- | ----- |
| 0x01 | Mouse Move   | dx      | dy    |
| 0x02 | Mouse Button | button  | state |
| 0x03 | Key Event    | keycode | state |
| 0x04 | Switch       | state   | 0     |

---

### 5.3 关键约定

* 所有坐标为**相对移动**
* 所有事件保持顺序
* Server 是唯一状态权威

---

## 6. 输入映射策略

### 6.1 键盘

* Linux evdev `KEY_*`
* 转换为 Windows Virtual-Key
* 使用映射表（静态数组）

### 6.2 鼠标

* REL_X / REL_Y → SendInput dx/dy
* BTN_* → MOUSEEVENTF_LEFTDOWN 等

---

## 7. 权限与安全

### 7.1 Linux

* 需要：

  * root
  * 或 udev rule 授权访问 `/dev/input`

### 7.2 Windows

* 普通用户即可
* 不安装驱动
* 不使用 hook

---

## 8. 启动与运行流程

### 8.1 Server（Ubuntu）

```text
1. 打开 evdev 设备
2. 初始化网络监听
3. 初始化状态机
4. 进入主循环
```

### 8.2 Client（Windows）

```text
1. 连接 Server
2. 进入事件接收循环
3. 调用 SendInput 注入
```

---

## 9. 配置管理
提供至少
1. 屏幕位置指示：Left/Right/Top/Down。以便告知鼠标穿越哪边屏幕。

---

## 10. 异常处理

| 场景        | 行为         |
| --------- | ---------- |
| 网络断开      | 自动回到 LOCAL |
| Client 崩溃 | 忽略远程状态     |
| 卡键        | 状态切换时强制释放  |

---

## 11. 性能目标

| 指标     | 目标      |
| ------ | ------- |
| 输入延迟   | < 5 ms  |
| CPU 占用 | < 1%    |
| 内存     | < 10 MB |

---


## 12. 开发约束
开发语言：c
编译管理：Cmake
版本管理：Git

