# BinAnalyzer Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add practical BIN analysis helpers: current-offset data inspection, checksums/hashes, drag-and-drop loading, and copy-as-C-array.

**Architecture:** Put numeric parsing and checksum/hash logic in small core services so they can be tested without GUI events. Reuse `HexViewerWidget::offsetChanged` as the current-offset signal and keep `BinAnalyzerWidget` responsible for UI wiring. Add C-array formatting to `DataConvertService` because it is a byte-format output helper used by multiple tools.

**Tech Stack:** Qt 6, CMake, C++17, Qt Test.

---

## File Structure

- Create `src/core/services/BinDataInspectorService.h`: endian enum, row/result structs, and current-offset value parsing API.
- Create `src/core/services/BinDataInspectorService.cpp`: integer/float/double parsing with bounds checks.
- Create `src/core/services/BinChecksumService.h`: checksum/hash result API.
- Create `src/core/services/BinChecksumService.cpp`: CheckSum-8/16/32, CRC-8/16/32, MD5, SHA-1.
- Modify `src/core/services/DataConvertService.h`: declare `formatCArray`.
- Modify `src/core/services/DataConvertService.cpp`: implement byte array formatter.
- Modify `src/core/CMakeLists.txt`: compile new services into `est_core`.
- Modify `tests/CMakeLists.txt`: compile new services into `est_app_widget_tests`.
- Modify `tests/app_widget_tests.cpp`: add service tests first.
- Modify `src/app/widgets/BinAnalyzerWidget.h`: add UI members, event handlers, current offset.
- Modify `src/app/widgets/BinAnalyzerWidget.cpp`: add inspector/checksum panel, drag/drop, copy-as-C-array action.

---

### Task 1: Service Tests

**Files:**
- Modify: `tests/app_widget_tests.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write failing tests**

Add includes near the top:

```cpp
#include "services/BinChecksumService.h"
#include "services/BinDataInspectorService.h"
```

Add test slots:

```cpp
void binDataInspectorParsesLittleAndBigEndianValues();
void binDataInspectorMarksUnavailableValuesAtEnd();
void binChecksumServiceCalculatesKnownVectors();
void dataConvertServiceFormatsCArray();
```

Add test bodies:

```cpp
void binDataInspectorParsesLittleAndBigEndianValues()
{
    const QByteArray bytes = QByteArray::fromHex("010203040000803f000000000000f03f");

    const auto littleRows = BinDataInspectorService::inspect(bytes, 0, BinDataInspectorService::Endian::Little);
    const auto bigRows = BinDataInspectorService::inspect(bytes, 0, BinDataInspectorService::Endian::Big);

    QCOMPARE(littleRows.value(QStringLiteral("uint16")), QStringLiteral("513 (0x0201)"));
    QCOMPARE(bigRows.value(QStringLiteral("uint16")), QStringLiteral("258 (0x0102)"));
    QCOMPARE(littleRows.value(QStringLiteral("uint32")), QStringLiteral("67305985 (0x04030201)"));
    QCOMPARE(bigRows.value(QStringLiteral("uint32")), QStringLiteral("16909060 (0x01020304)"));
    QCOMPARE(littleRows.value(QStringLiteral("float")), QStringLiteral("1"));
    QCOMPARE(littleRows.value(QStringLiteral("double")), QStringLiteral("-"));
}

void binDataInspectorMarksUnavailableValuesAtEnd()
{
    const QByteArray bytes = QByteArray::fromHex("AA BB");

    const auto rows = BinDataInspectorService::inspect(bytes, 1, BinDataInspectorService::Endian::Little);

    QCOMPARE(rows.value(QStringLiteral("uint8")), QStringLiteral("187 (0xBB)"));
    QCOMPARE(rows.value(QStringLiteral("uint16")), QStringLiteral("-"));
    QCOMPARE(rows.value(QStringLiteral("double")), QStringLiteral("-"));
}

void binChecksumServiceCalculatesKnownVectors()
{
    const QByteArray digits("123456789");
    const auto checks = BinChecksumService::calculate(digits);

    QCOMPARE(checks.value(QStringLiteral("CheckSum-8")), QStringLiteral("0xDD"));
    QCOMPARE(checks.value(QStringLiteral("CheckSum-16")), QStringLiteral("0x01DD"));
    QCOMPARE(checks.value(QStringLiteral("CRC-8")), QStringLiteral("0xF4"));
    QCOMPARE(checks.value(QStringLiteral("CRC-16/CCITT-FALSE")), QStringLiteral("0x29B1"));
    QCOMPARE(checks.value(QStringLiteral("CRC-32/IEEE")), QStringLiteral("0xCBF43926"));

    const auto hashes = BinChecksumService::calculate(QByteArrayLiteral("abc"));
    QCOMPARE(hashes.value(QStringLiteral("MD5")), QStringLiteral("900150983CD24FB0D6963F7D28E17F72"));
    QCOMPARE(hashes.value(QStringLiteral("SHA-1")), QStringLiteral("A9993E364706816ABA3E25717850C26C9CD0D89D"));
}

void dataConvertServiceFormatsCArray()
{
    const QString text = DataConvertService::formatCArray(QByteArray::fromHex("0102ff"), QStringLiteral("firmware"), 2);

    QCOMPARE(text, QStringLiteral("const uint8_t firmware[] = {\n    0x01, 0x02,\n    0xFF\n};"));
}
```

- [ ] **Step 2: Update test CMake for missing source files**

Add new source files to `tests/CMakeLists.txt`:

```cmake
    ../src/core/services/BinChecksumService.cpp
    ../src/core/services/BinDataInspectorService.cpp
```

- [ ] **Step 3: Run tests and verify RED**

Run:

```powershell
cmake --build build --target est_app_widget_tests
ctest --test-dir build --output-on-failure -R est_app_widget_tests
```

Expected: build fails because the new service headers/classes/functions do not exist.

---

### Task 2: Core Services

**Files:**
- Create: `src/core/services/BinDataInspectorService.h`
- Create: `src/core/services/BinDataInspectorService.cpp`
- Create: `src/core/services/BinChecksumService.h`
- Create: `src/core/services/BinChecksumService.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Implement `BinDataInspectorService`**

Expose:

```cpp
class BinDataInspectorService
{
public:
    enum class Endian { Little, Big };
    static QMap<QString, QString> inspect(QByteArrayView data, qsizetype offset, Endian endian);
};
```

Implementation rules:

- Return `"-"` if `offset + width > data.size()`.
- Use two's-complement casts for signed integer display.
- Format integers as `"decimal (0xHEX)"`.
- Format float/double with `QLocale::c().toString(value, 'g', 8)` and `QLocale::c().toString(value, 'g', 12)`.

- [ ] **Step 2: Implement `BinChecksumService`**

Expose:

```cpp
class BinChecksumService
{
public:
    static QMap<QString, QString> calculate(QByteArrayView data);
};
```

Return keys:

- `CheckSum-8`
- `CheckSum-16`
- `CheckSum-32`
- `CRC-8`
- `CRC-16/CCITT-FALSE`
- `CRC-32/IEEE`
- `MD5`
- `SHA-1`

- [ ] **Step 3: Register services in core CMake**

Add `.cpp` files to source list and `.h` files to header list in `src/core/CMakeLists.txt`.

- [ ] **Step 4: Run tests and verify Task 1 passes except C-array**

Run:

```powershell
cmake --build build --target est_app_widget_tests
ctest --test-dir build --output-on-failure -R est_app_widget_tests
```

Expected: service tests compile and pass; C-array test still fails until Task 3.

---

### Task 3: C Array Formatter

**Files:**
- Modify: `src/core/services/DataConvertService.h`
- Modify: `src/core/services/DataConvertService.cpp`

- [ ] **Step 1: Add declaration**

```cpp
static QString formatCArray(const QByteArray &bytes,
                            const QString &arrayName = QStringLiteral("data"),
                            int bytesPerLine = 12);
```

- [ ] **Step 2: Add implementation**

Implementation details:

- Empty array still emits a valid empty initializer.
- Sanitize invalid array names to `data`.
- Use uppercase two-digit hex bytes.
- Indent data lines with four spaces.

- [ ] **Step 3: Run tests and verify GREEN**

Run:

```powershell
cmake --build build --target est_app_widget_tests
ctest --test-dir build --output-on-failure -R est_app_widget_tests
```

Expected: all service and formatter tests pass.

---

### Task 4: BinAnalyzer Widget UI

**Files:**
- Modify: `src/app/widgets/BinAnalyzerWidget.h`
- Modify: `src/app/widgets/BinAnalyzerWidget.cpp`
- Modify: `src/app/CMakeLists.txt`

- [ ] **Step 1: Add UI members and helper methods**

Add members for:

- current offset
- endian combo
- inspector table
- checksum scope combo
- checksum length spin box
- checksum result text edit

Add methods:

- `void refreshInspector();`
- `void calculateChecksums();`
- `void copyCArrayToClipboard();`
- `void dragEnterEvent(QDragEnterEvent *event) override;`
- `void dropEvent(QDropEvent *event) override;`

- [ ] **Step 2: Add right-side tool panel**

Place a vertical tool panel beside `StringExtractPanel` inside the existing splitter. The panel contains a `QTableWidget` for inspector rows and a read-only `QPlainTextEdit` for checksum results.

- [ ] **Step 3: Wire current offset**

In `updateDetail`, store `m_currentOffset = offset` and call `refreshInspector()`.

- [ ] **Step 4: Wire checksum calculation**

Calculate over whole file or `m_loader.data().mid(m_currentOffset, length)` and display each result as `name: value`.

- [ ] **Step 5: Wire drag/drop**

Accept one local file URL and call `loadFile(path)`. Reject multiple files with `statusMessageGenerated("一次只能拖入一个 BIN 文件；文件对比将在后续版本支持。")`.

- [ ] **Step 6: Wire copy C array**

Use `DataConvertService::formatCArray(m_loader.data(), "data")`, put text on the clipboard, and emit a status message.

- [ ] **Step 7: Build app target**

Run:

```powershell
cmake --build build --target est_app
```

Expected: app target builds.

---

## Self-Review

Spec coverage:

- Data Inspector: Task 1, Task 2, Task 4.
- Endianness switch: Task 1, Task 2, Task 4.
- Checksum/hash: Task 1, Task 2, Task 4.
- Drag-and-drop loading: Task 4.
- Copy C array: Task 1, Task 3, Task 4.

No placeholders remain in the implementation steps. Type names are consistent with the proposed files and API.
