@echo off
echo Building LanKM Client for Windows...

REM Create build directory
if not exist build_windows mkdir build_windows
cd build_windows

REM Configure with Visual Studio
echo Configuring with CMake...
cmake .. -G "Visual Studio 15 2017" -A x64
if %ERRORLEVEL% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

REM Build the project
echo Building...
cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo Client executable: build_windows\Release\lankm-client.exe
echo.
echo Usage: lankm-client.exe <server-ip>
echo Example: lankm-client.exe 192.168.1.100
pause