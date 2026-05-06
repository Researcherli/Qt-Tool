# Embedded Software Tools

Last updated: 2026-05-05

## Repo shape
- Qt 6 / CMake / C++17 project.
- `est_core` is a static library in `src/core`.
- `est_app` is the GUI executable in `src/app`.

## App entry + navigation
- Main entrypoint: `src/app/main.cpp`.
- `main()` creates `est::AppShell`, loads `:/themes/wood_classic.qss`, then shows the window.
- Real top-level pages in `AppShell`: `HomePage`, `SerialAssistantPage`, `DataConvertPage`, `BinAnalyzerPage`.
- Home page buttons navigate to those pages; BIN analysis also opens the file dialog.

## Build + tests
- Top-level CMake enables `CMAKE_AUTOMOC`, `CMAKE_AUTOUIC`, `CMAKE_AUTORCC`.
- Tests are built only when `BUILD_TESTING=ON`.
- When enabled, CMake calls `enable_testing()`, finds `Qt6::Test`, and adds `tests/`.
- Test target: `est_app_widget_tests`; it links `Qt6::Core`, `Qt6::Widgets`, `Qt6::SerialPort`, and `Qt6::Test`.

## Plugins
- `est_app` loads plugins from `applicationDirPath()/plugins`.
- In dev builds, `EST_PLUGIN_DIR` is added as an extra plugin search path.
- `EST_PLUGIN_DIR` is only compiled into `est_app` when defined by CMake.

## Serial tool facts that matter
- Serial transport type is `serial` (`SerialTransport`).
- Open config keys used by code: `portName`, `baudRate`, `dataBits`, `parity`, `stopBits`.
- `portName` is required; open fails if it is empty.
- Flow control is always set to `NoFlowControl`.
- The serial page refreshes ports from `QSerialPortInfo::availablePorts()` and persists recent serial profiles on successful connect.

## Recent records
- Stored as `recent_records.json` under `QStandardPaths::AppDataLocation`.
- Fallback path if AppDataLocation is empty: `~/EmbeddedSoftwareTools/recent_records.json`.
- Keys in storage: `serialProfiles`, `binFiles`, `searchKeywords`.
- Each list is capped at 10 entries and de-duplicates by its key fields.

## Resources
- `src/resources/resources.qrc` is part of the app build.
- The qrc bundles `themes/industrial_dark.qss` and `themes/wood_classic.qss`.
