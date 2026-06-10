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

# === CPack 安装包配置 ===
set(CPACK_PACKAGE_NAME "EST_Studio")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VENDOR "Embedded Software Tools")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "嵌入式软件开发工具集 - CAN总线分析、协议解码、RTOS监控、串口终端、波形分析")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "EST Studio")
set(CPACK_GENERATOR "NSIS")
set(CPACK_NSIS_DISPLAY_NAME "EST Studio")
set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
set(CPACK_NSIS_PACKAGE_NAME "EST Studio")
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/src/resources/icons/app_icon_256.png")
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/src/resources/icons/app_icon_256.png")
set(CPACK_NSIS_CREATE_ICONS_EXTRA "
    CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\EST Studio.lnk' '$INSTDIR\\\\bin\\\\EST_Studio.exe'
    CreateShortCut '$DESKTOP\\\\EST Studio.lnk' '$INSTDIR\\\\bin\\\\EST_Studio.exe'
")
set(CPACK_NSIS_DELETE_ICONS_EXTRA "
    Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\EST Studio.lnk'
    Delete '$DESKTOP\\\\EST Studio.lnk'
")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH OFF)

include(CPack)

# === 测试 ===
option(BUILD_TESTING "Build tests" OFF)
if(BUILD_TESTING)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
