cmake_minimum_required(VERSION 3.15)
project(lankm VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable compile options
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Werror)
endif()

# Linux-specific definitions
if(UNIX AND NOT APPLE)
    add_definitions(-D_GNU_SOURCE)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Common source files
set(COMMON_SOURCES
    src/common/protocol.c
)

# Find required libraries (Linux only)
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBEVDEV libevdev)
    pkg_check_modules(X11 x11)

    if(LIBEVDEV_FOUND AND X11_FOUND)
        # Add include directories
        include_directories(${LIBEVDEV_INCLUDE_DIRS})
        include_directories(${X11_INCLUDE_DIRS})
        set(PLATFORM_LIBS ${LIBEVDEV_LIBRARIES} ${X11_LIBRARIES})
        set(CAN_BUILD TRUE)
    else()
        message(WARNING "libevdev or X11 not found. Cannot build.")
        set(CAN_BUILD FALSE)
    endif()
else()
    message(WARNING "pkg-config not found. Cannot build.")
    set(CAN_BUILD FALSE)
endif()

# Server executable
if(CAN_BUILD)
    add_executable(lankm-server
        src/server/main.c
        src/server/input_capture.c
        src/server/state_machine.c
        src/server/keyboard_state.c
        ${COMMON_SOURCES}
    )

    target_link_libraries(lankm-server ${PLATFORM_LIBS} pthread)

    # Install target
    install(TARGETS lankm-server DESTINATION bin)
endif()

# Enable testing
enable_testing()

# Custom target for formatting
find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i src/**/*.c src/**/*.h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting source code with clang-format"
    )
endif()