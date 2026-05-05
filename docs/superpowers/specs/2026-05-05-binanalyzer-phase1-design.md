# BinAnalyzer Phase 1 Design

## Goal

Improve the BIN analysis workflow with practical embedded-development helpers while keeping the first implementation small enough to fit the current widget structure.

## Scope

Phase 1 includes:

- Data Inspector for values starting at the current HEX offset.
- Endianness switch for multi-byte numeric interpretation.
- Checksum and hash calculation for the whole file and for a byte range starting at the current offset.
- Drag-and-drop loading for one BIN file.
- Copy-as-C-array export for the loaded file.

Phase 1 does not include:

- Byte-range selection in the HEX view.
- Dual-file diff view.
- Struct parser.
- Bookmark management.

Those features need a stronger byte selection and document model than the current `QPlainTextEdit`-based `HexViewerWidget` provides.

## Architecture

Add focused service logic in `src/core/services` so numeric inspection, checksums, and C-array formatting can be tested without Qt widget interaction. Keep `BinAnalyzerWidget` responsible for UI composition and wiring only.

The current `HexViewerWidget` emits `offsetChanged(qint64, uchar, QString)`. Phase 1 will reuse that signal as the single source of cursor state. The inspector and checksum panel update from `m_currentOffset` and `m_loader.data()`.

## Components

### `BinDataInspectorService`

Provides a value report for data at a byte offset:

- `int8`, `uint8`
- `int16`, `uint16`
- `int32`, `uint32`
- `int64`, `uint64`
- `float`
- `double`

For insufficient bytes, the service returns a short unavailable marker instead of reading past the end. Multi-byte values use either little-endian or big-endian.

### `BinChecksumService`

Provides checksum and hash results over a `QByteArrayView`:

- CheckSum-8: sum of bytes masked to 8 bits.
- CheckSum-16: sum of bytes masked to 16 bits.
- CheckSum-32: sum of bytes masked to 32 bits.
- CRC-8: polynomial `0x07`, initial `0x00`.
- CRC-16/CCITT-FALSE: polynomial `0x1021`, initial `0xFFFF`.
- CRC-32/IEEE: polynomial `0x04C11DB7` reflected implementation, initial `0xFFFFFFFF`, final xor `0xFFFFFFFF`.
- MD5.
- SHA-1.

### `DataConvertService` C Array Helper

Add a formatter for C/C++ byte arrays:

```cpp
static QString formatCArray(const QByteArray &bytes,
                            const QString &arrayName,
                            int bytesPerLine = 12);
```

It emits:

```cpp
const uint8_t data[] = {
    0x01, 0x02
};
```

### `BinAnalyzerWidget`

Add a right-side tool panel next to the existing string panel with two groups:

- Data Inspector: endianness combo and read-only value table.
- Checksum & Hash: scope combo, length input, calculate button, read-only results.

Add drag-and-drop support to `BinAnalyzerWidget`. A single local file loads through `loadFile()`. Multiple dropped files are rejected in phase 1 with a status message.

Add a copy button and HEX viewer context action for "Copy C Array". Phase 1 copies the entire loaded file.

## User Interaction

When a file loads, the HEX view jumps to offset `0`, and the inspector shows decoded values from offset `0`.

When the user jumps through search results or string extraction, `offsetChanged` refreshes the inspector. The checksum panel keeps its configured scope and length until the user recalculates.

Checksum scope options:

- Entire file.
- From current offset with length.

If range length is zero or extends past the file end, the widget reports a validation message and does not calculate.

## Testing

Add service tests to `tests/app_widget_tests.cpp` first:

- Endianness-specific integer parsing.
- Float/double parsing for known IEEE-754 byte sequences.
- Insufficient-byte handling near the end of data.
- CheckSum-8/16/32 on known bytes.
- CRC-8, CRC-16/CCITT-FALSE, CRC-32 on `"123456789"`.
- MD5 and SHA-1 on `"abc"`.
- C-array formatting.

Add widget smoke tests only where stable:

- `BinAnalyzerWidget` accepts drops is mostly event-driven and can be covered later after service coverage.
- Verify the new controls exist by object name.

## Acceptance

- Current offset displays int/uint/float/double interpretations.
- Endianness switch changes multi-byte values.
- Whole-file checksum/hash calculation works.
- Offset-range checksum/hash calculation works.
- Dragging one file into the BinAnalyzer page loads it.
- Copy-as-C-array produces valid C/C++ byte array text.
