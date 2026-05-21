cmake_minimum_required(VERSION 3.15)
project(onekm VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Werror)
endif()

if(UNIX AND NOT APPLE)
    add_definitions(-D_GNU_SOURCE)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)

set(COMMON_SOURCES
    src/common/protocol.c
)

find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBEVDEV libevdev)
    pkg_check_modules(X11 x11)
    pkg_check_modules(LIBUDEV libudev)

    if(LIBEVDEV_FOUND AND X11_FOUND)
        include_directories(
            ${LIBEVDEV_INCLUDE_DIRS}
            ${X11_INCLUDE_DIRS}
        )
        set(PLATFORM_LIBS
            ${LIBEVDEV_LIBRARIES}
            ${X11_LIBRARIES}
        )
        if(LIBUDEV_FOUND)
            include_directories(${LIBUDEV_INCLUDE_DIRS})
            list(APPEND PLATFORM_LIBS ${LIBUDEV_LIBRARIES})
            add_definitions(-DHAVE_LIBUDEV)
            message(STATUS "libudev found — hotplug enabled")
        else()
            message(WARNING "libudev not found — keyboard hotplug disabled")
            message(WARNING "  sudo apt-get install libudev-dev")
        endif()
        set(CAN_BUILD TRUE)
    else()
        message(WARNING "Missing required dependencies: libevdev, x11")
        message(WARNING "  sudo apt-get install libevdev-dev libx11-dev")
        set(CAN_BUILD FALSE)
    endif()
else()
    message(WARNING "pkg-config not found")
    set(CAN_BUILD FALSE)
endif()

if(CAN_BUILD)
    add_executable(onekm-server
        src/server/main.c
        src/server/input_capture.c
        src/server/uinput_inject.c
        src/server/hotplug.c
        src/server/inhibit.c
        src/server/uart.c
        src/server/state_machine.c
        src/server/keyboard_state.c
        ${COMMON_SOURCES}
    )

    target_link_libraries(onekm-server ${PLATFORM_LIBS} pthread)

    install(TARGETS onekm-server DESTINATION bin)
endif()

enable_testing()

find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i src/**/*.c src/**/*.h
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Formatting source code"
    )
endif()
