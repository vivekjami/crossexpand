cmake_minimum_required(VERSION 3.16)
project(CrossExpand VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED x11 xtst)
find_package(Threads REQUIRED)

# Check for UUID library
find_library(UUID_LIB uuid)
if(NOT UUID_LIB)
    message(FATAL_ERROR "UUID library not found")
endif()

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
    src/core/event_queue.cpp
    src/core/advanced_template_engine.cpp
    src/core/enhanced_text_injector.cpp
    src/core/memory_pool.cpp
    src/core/plugin_system.cpp
    src/utils/logger.cpp
    src/utils/config_manager.cpp
    src/utils/performance_monitor.cpp
)

# Day 3 GUI sources
set(GUI_SOURCES
    src/gui/web_server.cpp
    src/gui/web_gui.cpp
)

# Plugin sources (built-in plugins)
set(PLUGIN_SOURCES
    src/plugins/code_snippet_plugin.cpp
)

# Platform source files  
set(PLATFORM_SOURCES
    src/platform/linux/x11_input_manager.cpp
    src/platform/linux/x11_text_injector.cpp
)

# Day 2 demo executable
add_executable(demo_day2
    src/demo_day2.cpp
    ${CORE_SOURCES}
)

target_link_libraries(demo_day2
    PRIVATE
    ${X11_LIBRARIES}
    ${UUID_LIB}
    Threads::Threads
    nlohmann_json::nlohmann_json
)

target_include_directories(demo_day2 PRIVATE ${X11_INCLUDE_DIRS})
target_compile_options(demo_day2 PRIVATE ${X11_CFLAGS_OTHER})

# Simplified Day 2 demo executable
add_executable(demo_day2_simple
    src/demo_day2_simple.cpp
    ${CORE_SOURCES}
)

target_link_libraries(demo_day2_simple
    PRIVATE
    ${X11_LIBRARIES}
    ${UUID_LIB}
    Threads::Threads
    nlohmann_json::nlohmann_json
)

target_include_directories(demo_day2_simple PRIVATE ${X11_INCLUDE_DIRS})
target_compile_options(demo_day2_simple PRIVATE ${X11_CFLAGS_OTHER})

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
    ${UUID_LIB}
    Threads::Threads
    nlohmann_json::nlohmann_json
)

target_include_directories(crossexpand PRIVATE ${X11_INCLUDE_DIRS})
target_compile_options(crossexpand PRIVATE ${X11_CFLAGS_OTHER})

# Day 3 complete application
add_executable(crossexpand_day3
    src/main_day3.cpp
    ${CORE_SOURCES}
    ${GUI_SOURCES}
    ${PLUGIN_SOURCES}
)

target_link_libraries(crossexpand_day3
    PRIVATE
    ${X11_LIBRARIES}
    ${UUID_LIB}
    Threads::Threads
    nlohmann_json::nlohmann_json
    dl  # For dynamic loading (dlopen)
)

target_include_directories(crossexpand_day3 PRIVATE ${X11_INCLUDE_DIRS})
target_compile_options(crossexpand_day3 PRIVATE ${X11_CFLAGS_OTHER})

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

# Enhanced test executable
add_executable(test_advanced
    tests/test_advanced.cpp
    ${CORE_SOURCES}
)

target_link_libraries(test_advanced
    PRIVATE
    ${X11_LIBRARIES}
    ${UUID_LIB}
    Threads::Threads
    nlohmann_json::nlohmann_json
)

target_include_directories(test_advanced PRIVATE ${X11_INCLUDE_DIRS})
target_compile_options(test_advanced PRIVATE ${X11_CFLAGS_OTHER})

# Day 3 simple test
add_executable(test_day3_simple
    src/test_day3_simple.cpp
    ${CORE_SOURCES}
)

target_link_libraries(test_day3_simple
    PRIVATE
    nlohmann_json::nlohmann_json
)

add_test(NAME AdvancedTests COMMAND test_advanced)
add_test(NAME Day2Demo COMMAND demo_day2)
add_test(NAME Day3SimpleTest COMMAND test_day3_simple)
