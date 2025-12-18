# Building LanKM Client on Windows

## Prerequisites
- Visual Studio 2015 or later (Community edition works fine)
- CMake 3.15 or later

## Quick Build Steps

1. Open Command Prompt or PowerShell
2. Navigate to the LanKM directory
3. Run the provided batch script:
   ```
   build_windows.bat
   ```
4. The executable will be created at: `build_windows\Release\lankm-client.exe`

## Manual Build Steps

1. Open Developer Command Prompt for VS
2. Navigate to LanKM directory
3. Create build directory:
   ```
   mkdir build_windows
   cd build_windows
   ```
4. Configure with CMake:
   ```
   cmake .. -G "Visual Studio 15 2017" -A x64
   ```
   Note: Adjust VS version if needed (e.g., "Visual Studio 16 2019")
5. Build:
   ```
   cmake --build . --config Release
   ```

## Running the Client

```
lankm-client.exe <server-ip> [port]

Examples:
lankm-client.exe 192.168.1.100
lankm-client.exe 192.168.1.100 24800
```

## Testing the System

1. Start the server on Linux:
   ```
   sudo ./lankm-server
   ```

2. Start the client on Windows:
   ```
   lankm-client.exe <linux-server-ip>
   ```

3. Move mouse to screen edge to switch control
4. Press ESC to switch back to local control

## Troubleshooting

- **Connection Refused**: Check firewall settings on server (port 24800)
- **No Input Events**: Ensure running with appropriate privileges
- **Build Errors**: Make sure Visual Studio C++ tools are installed