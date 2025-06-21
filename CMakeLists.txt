cmake_minimum_required(VERSION 3.16)
project(CrossExpand VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED x11 xtst)
find_package(Threads REQUIRED)

# Fetch nlohmann/json
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories
include_directories(include)
include_directories(src)

# Core source files
set(CORE_SOURCES
    src/core/template_engine.cpp
    src/utils/logger.cpp
    src/utils/config_manager.cpp
)

# Platform source files
set(PLATFORM_SOURCES
    src/platform/linux/x11_input_manager.cpp
    src/platform/linux/x11_text_injector.cpp
)

# Main executable
add_executable(crossexpand
    src/main.cpp
    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
)

# Link libraries
target_link_libraries(crossexpand
    PRIVATE
    ${X11_LIBRARIES}
    Threads::Threads
    nlohmann_json::nlohmann_json
)

target_include_directories(crossexpand PRIVATE ${X11_INCLUDE_DIRS})
target_compile_options(crossexpand PRIVATE ${X11_CFLAGS_OTHER})

# Enable testing
enable_testing()

# Simple test executable
add_executable(test_basic
    tests/test_basic.cpp
    ${CORE_SOURCES}
)

target_link_libraries(test_basic
    PRIVATE
    nlohmann_json::nlohmann_json
)

add_test(NAME BasicTests COMMAND test_basic)
