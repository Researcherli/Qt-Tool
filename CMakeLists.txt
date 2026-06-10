cmake_minimum_required(VERSION 3.19)
project(EmbeddedSoftwareTools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MSVC: UTF-8 encoding + strict conformance (Qt 6.11 requires /permissive-).
# /FS keeps parallel Debug builds from racing on the shared PDB.
if(MSVC)
    add_compile_options(/utf-8 /permissive- /Zc:__cplusplus /FS)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 启用链接时优化 (LTO/IPO)
include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT output)
if(result)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# 组织输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets SerialPort Charts)

# === 核心库 ===
add_subdirectory(src/core)

# === 应用壳 ===
add_subdirectory(src/app)

# === 插件（可选编译） ===
# 插件尚未实现，以下选项预留供未来使用
# option(ENABLE_PLUGIN_CAN_ANALYZER    "Build CAN Analyzer plugin"    ON)
# option(ENABLE_PLUGIN_SERIAL_TERMINAL "Build Serial Terminal plugin"  ON)
# option(ENABLE_PLUGIN_PROTOCOL        "Build Protocol Decoder plugin" ON)
# option(ENABLE_PLUGIN_FLASHER         "Build Firmware Flasher plugin" OFF)
# option(ENABLE_PLUGIN_INSTRUMENT      "Build Instrument Panel plugin" OFF)

# === 测试 ===
option(BUILD_TESTING "Build tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
