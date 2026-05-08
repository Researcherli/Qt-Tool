# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**Embedded Software Tools (EST Platform)** — A Qt 6 / CMake / C++17 GUI tool suite for embedded development (CAN analysis, serial debugging, protocol decoding, firmware flashing, binary inspection).

- **Build environment**: MSVC2022 (`F:\Microsoft Visual Studio\18`) + Qt 6.11.0 (`F:\Qt\6.11.0\msvc2022_64`) + JOM
- **Namespace**: `est::`
- **Two build outputs**: `est_core` (static lib) + `est_app` (WIN32 executable)

## Build Commands

Must call `vcvarsall.bat x64` before building in any fresh terminal:

```bat
# Configure (MSVC, Release)
cmake -B build -G "NMake Makefiles JOM" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=F:/Qt/6.11.0/msvc2022_64

# Build
cmake --build build

# Configure + build with tests
cmake -B build -DBUILD_TESTING=ON
cmake --build build

# Run all tests
ctest --test-dir build

# Run a single test executable directly
build\bin\est_app_widget_tests.exe
build\bin\est_file_compare_engine_tests.exe
build\bin\est_file_compare_page_tests.exe
```

Qt Creator uses the `build/Desktop_Qt_6_11_0_MSVC2022_64bit-Release/` build directory.

## Architecture

### Layer Structure

```
est_app  (src/app/)
  └── est_core  (src/core/)  [static lib]
        ├── databus/        — DataBus pub/sub message broker
        ├── transport/      — ITransport abstraction + SerialTransport
        ├── plugin/         — IPlugin / ICore / IViewFactory / PluginRegistry
        └── services/       — stateless business logic (BinFile, DataConvert, RecentRecords, etc.)
```

### Key Interfaces

- **`ICore`** — Passed to plugins at `initialize()`. Provides access to `DataBus`, `PluginRegistry`, `TransportRegistry`, `RecentRecordManager`. Implemented by `AppShell`.
- **`ITransport`** — Abstraction for all data sources (serial, file replay, hardware adapters). Open config is a `QVariantMap`.
- **`DataBus`** — Thread-safe pub/sub hub. Channel naming convention:
  - `transport.<type>.<instance>` — raw transport data
  - `protocol.<type>.<channel>` — decoded protocol data
  - `plugin.<name>.<signal>` — plugin-defined signals

### App Shell & Navigation

- `main.cpp` → creates `est::AppShell`, loads `:/themes/wood_classic.qss`, shows window
- `AppShell` implements `ICore` and owns top-level infrastructure (DataBus, registries)
- Navigation: `SideNavBar` (left, custom-painted `NavIconButton`s, collapses to 72px / expands to 200px with animation) + `QStackedWidget` (right)
- Pages: `HomePage`, `SerialAssistantPage`, `DataConvertPage`, `BinAnalyzerPage`, `FileComparePage`, `MemoryAnalyzerPage`

### Plugin System

- Plugins live in `applicationDirPath()/plugins` at runtime
- In dev builds, `EST_PLUGIN_DIR` CMake variable adds a second search path
- CMake options: `ENABLE_PLUGIN_CAN_ANALYZER`, `ENABLE_PLUGIN_SERIAL_TERMINAL`, `ENABLE_PLUGIN_PROTOCOL`, `ENABLE_PLUGIN_FLASHER`, `ENABLE_PLUGIN_INSTRUMENT`

### Persistent State

- Recent records stored as `recent_records.json` in `QStandardPaths::AppDataLocation`
- Keys: `serialProfiles`, `binFiles`, `searchKeywords` — each capped at 10, de-duplicated

### Serial Transport

- Transport type ID: `"serial"` (`SerialTransport`)
- Required open config key: `portName` (open fails if empty)
- Config keys: `portName`, `baudRate`, `dataBits`, `parity`, `stopBits`
- Flow control is always `NoFlowControl`

## Test Targets

| Target | What it tests |
|---|---|
| `est_app_widget_tests` | Widget/page smoke tests, service unit tests |
| `est_file_compare_engine_tests` | `FileCompareEngine` diff algorithm |
| `est_file_compare_page_tests` | `FileComparePage` + `DiffView` widget |

Tests compile sources directly (not linked against `est_core`) — when adding new services, add them to both `src/core/CMakeLists.txt` and `tests/CMakeLists.txt`.

## Resources

- `src/resources/resources.qrc` — bundles QSS themes: `themes/industrial_dark.qss`, `themes/wood_classic.qss`
- App icon: `src/app/app_icon.rc` (Windows only)

## CMake Notes

- `CMAKE_AUTOMOC/AUTOUIC/AUTORCC` are all `ON`
- MSVC compile flags: `/utf-8 /permissive- /Zc:__cplusplus`
- LTO/IPO enabled when supported
- Output directories: `bin/`, `plugins/`, `lib/` under `CMAKE_BINARY_DIR`
