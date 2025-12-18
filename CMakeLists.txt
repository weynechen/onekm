cmake_minimum_required(VERSION 3.15)
project(lankm VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Enable compile options
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Werror)
endif()

# Platform detection
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DWINVER=0x0601)  # Windows 7
    add_definitions(-D_WIN32_WINNT=0x0601)
elseif(UNIX AND NOT APPLE)
    add_definitions(-D_GNU_SOURCE)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

# Common source files
set(COMMON_SOURCES
    src/common/protocol.c
)

# Common headers
set(COMMON_HEADERS
    src/common/protocol.h
)

# Find required libraries
if(WIN32)
    # Windows only needs win32 API
    set(PLATFORM_LIBS ws2_32 user32)
else()
    # Linux needs libevdev and X11 (optional if not building server)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBEVDEV libevdev)
        pkg_check_modules(X11 x11)

        if(LIBEVDEV_FOUND AND X11_FOUND)
            # Add include directories
            include_directories(${LIBEVDEV_INCLUDE_DIRS})
            include_directories(${X11_INCLUDE_DIRS})
            set(PLATFORM_LIBS ${LIBEVDEV_LIBRARIES} ${X11_LIBRARIES})
            set(CAN_BUILD_SERVER TRUE)
        else()
            message(WARNING "libevdev or X11 not found. Server component will not be built.")
            set(CAN_BUILD_SERVER FALSE)
        endif()
    else()
        message(WARNING "pkg-config not found. Server component will not be built.")
        set(CAN_BUILD_SERVER FALSE)
    endif()
endif()

# Server executable (Linux only)
if(UNIX AND NOT APPLE AND CAN_BUILD_SERVER)
    add_executable(lankm-server
        src/server/main.c
        src/server/input_capture.c
        src/server/edge_detector.c
        src/server/state_machine.c
        ${COMMON_SOURCES}
    )

    target_link_libraries(lankm-server ${PLATFORM_LIBS} pthread)

    # Install target
    install(TARGETS lankm-server DESTINATION bin)
endif()

# Client executable (Windows only)
if(WIN32)
    add_executable(lankm-client
        src/client/main.c
        src/client/input_inject.c
        src/client/keymap.c
        ${COMMON_SOURCES}
    )

    target_link_libraries(lankm-client ${PLATFORM_LIBS})

    # Install target
    install(TARGETS lankm-client DESTINATION bin)
endif()

# Add a simple test executable when main components can't be built
if(NOT CAN_BUILD_SERVER AND NOT WIN32)
    add_executable(test-protocol
        test_build.c
        src/common/protocol.c
    )
    target_include_directories(test-protocol PRIVATE ${CMAKE_SOURCE_DIR}/src)
    message(STATUS "Building test-protocol executable since main components require dependencies")
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