# LanKM - LAN Keyboard & Mouse Controller

LanKM是一个轻量级的网络键鼠共享系统，允许您使用一套键盘鼠标控制局域网内的两台计算机。

## 系统架构

- **Server (Ubuntu Linux)**: 连接物理键盘鼠标，负责输入捕获和控制决策
- **Client (Windows)**: 接收网络输入并注入到系统

## 主要特性

- 鼠标移动到屏幕边缘时自动切换控制权
- 极低延迟 (< 5ms)
- 轻量级设计 (CPU < 1%, 内存 < 10MB)
- 无GUI，无复杂配置
- 支持键盘和鼠标完整功能

## 构建要求

### Linux (Server)
- GCC 或 Clang
- CMake 3.15+
- libevdev
- X11 开发库

### Windows (Client)
- Visual Studio 2015+ 或 MinGW
- CMake 3.15+

## 安装依赖

### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install build-essential cmake libevdev-dev libx11-dev
```

### Windows
使用vcpkg或手动安装CMake。

## 编译

```bash
mkdir build
cd build
cmake ..
make

# 在Windows上使用Visual Studio
cmake --build . --config Release
```

## 使用方法

### Linux Server
```bash
# 需要root权限访问输入设备
sudo ./build/lankm-server
```

### Windows Client

#### 编译客户端：
```bash
# 方法1：使用提供的批处理脚本
build_windows.bat

# 方法2：手动编译
mkdir build_windows
cd build_windows
cmake .. -G "Visual Studio 15 2017" -A x64
cmake --build . --config Release
```

#### 运行客户端：
```bash
./build_windows/Release/lankm-client.exe <server-ip> [port]

# 示例
lankm-client.exe 192.168.1.100
lankm-client.exe 192.168.1.100 24800
```

## 权限配置

### Linux udev规则
创建 `/etc/udev/rules.d/99-lankm.rules`:
```
KERNEL=="event*", MODE="0666", GROUP="input"
```

然后重新加载udev规则：
```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## 协议

LanKM使用简单的二进制TCP协议：

```
struct Message {
    uint8_t type;    // 消息类型
    int16_t a;       // 参数1
    int16_t b;       // 参数2
};
```

消息类型：
- 0x01: 鼠标移动 (dx, dy)
- 0x02: 鼠标按键 (button, state)
- 0x03: 键盘按键 (keycode, state)
- 0x04: 控制切换 (state, 0)

## 配置

默认配置：
- TCP端口: 24800
- 屏幕边缘切换: 启用

## 故障排除

1. **权限不足**：确保Linux上有访问/dev/input/*的权限
2. **防火墙**：确保TCP端口24800未被阻止
3. **设备识别**：检查键盘鼠标是否被正确识别

## 开发

项目结构：
```
src/
├── common/          # 共用代码（协议、网络）
├── server/          # Linux服务端
└── client/          # Windows客户端
```

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request。