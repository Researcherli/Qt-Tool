# 文件比较功能优化实施计划

> 项目：Embedded Software Tools (Qt 6 / CMake / C++17)
> 涉及模块：`est_core` (FileCompareEngine) + `est_app` (FileComparePage / DiffMinimapWidget / CodeEditor)
> 总优化项：21 项，分 5 个阶段实施

---

## 优化项总览与优先级矩阵

| 编号 | 问题 | 严重度 | 阶段 | 预计工时 |
|------|------|--------|------|----------|
| 1 | 编辑器不支持行号显示 | 🔴 | P1 | 4h |
| 2 | DiffMinimap 绘制逻辑问题 | 🔴 | P1 | 3h |
| 3 | 大文件 diff 导致 UI 卡顿 | 🔴 | P1 | 6h |
| 4 | 滚动联动是"硬绑定" | 🔴 | P1 | 4h |
| 14 | maxExactLines 定义但未使用 | 🟢 | P2 | 1h |
| 15 | coalesceChangedRuns 贪心配对 | 🟢 | P2 | 4h |
| 20 | 缺少文件编码检测 | 🔵 | P2 | 4h |
| 6 | 二进制比较无字节级高亮 | 🟡 | P2 | 5h |
| 5 | 缺少"转到行"功能 | 🟡 | P3 | 4h |
| 7 | 交换后差异导航位置丢失 | 🟡 | P3 | 2h |
| 8 | 单文件预览模式缺提示 | 🟡 | P3 | 2h |
| 9 | 没有文件拖拽支持 | 🟡 | P3 | 4h |
| 17 | 缺少键盘快捷键 | 🟢 | P3 | 5h |
| 10 | 没有差异合并/编辑功能 | 🟡 | P4 | 12h |
| 16 | "只看差异"折叠行不可展开 | 🟢 | P4 | 3h |
| 13 | 工具栏按钮缺少图标 | 🟢 | P4 | 4h |
| 12 | 状态卡片"未选择"时单调 | 🟢 | P4 | 2h |
| 18 | 没有"导出差异"功能 | 🟢 | P4 | 5h |
| 19 | QTextEdit 不理想 | 🔵 | P5 | 16h |
| 21 | 没有最近的比较记录 | 🔵 | P5 | 4h |
| 11 | 编辑器字体无法调整 | 🟢 | P5 | 3h |

---

## 阶段一：核心修复 — 渲染与稳定性

> **目标**：修复影响基本可用性的 4 个严重问题，确保文件比较功能的渲染正确性和操作流畅性。
> **完成标准**：行号可见、Minimap 绘制正确、10000+ 行文件比较不卡顿、滚动同步平滑。
> **预计总工时**：~17h

### 1.1 编辑器行号显示修复（🔴 #1）

**问题根因**：
`CodeEditor` 虽然内置了 `LineNumberArea`，但当前 `lineNumberAreaPaintEvent` 使用 `Qt::lightGray` 硬编码背景色，与项目深色/工业风格主题不协调。更重要的是，当 `setReadOnly(true)` 时（比较页两个编辑器均为只读），`highlightCurrentLine()` 提前返回，且 viewport margin 在大量文本通过 `setPlainText` 一次性设置时可能未正确更新。

**技术方案**：

- **文件**：`src/app/widgets/CodeEditor.h` / `CodeEditor.cpp`
- **方案**：
  1. 在 `CodeEditor` 中添加 `setLineNumbersVisible(bool)` 公共方法，默认为 `true`
  2. 添加 `bool m_showLineNumbers` 成员变量
  3. 修改 `updateLineNumberAreaWidth(int)` — 当 `m_showLineNumbers` 为 false 时设 viewport margin 为 0
  4. 添加 `setLineNumberAreaBackground(const QColor &)` 和 `setLineNumberAreaTextColor(const QColor &)` 方法，使背景色/文字色可配置
  5. 在 `lineNumberAreaPaintEvent()` 中使用成员变量存储的颜色
  6. 修改 `resizeEvent()` — 行号区域使用 `fontMetrics().lineSpacing()` 而非 `blockBoundingRect` 提高精度
  7. 将背景色默认改为 `#2B2B2B`（匹配工业暗色主题），文字色改为 `#8A8A8A`
  8. 在 `FileComparePage::setupUi()` 中，创建编辑器后显式调用 `m_leftEditor->setLineNumbersVisible(true)` 以确保行号在比较页中显示
  9. 将`lineNumberArea` 的 `setAutoFillBackground(true)` 以避免被父控件背景覆盖
- **Qt API**：`QPlainTextEdit::setViewportMargins()`, `QFontMetrics::lineSpacing()`, `QFontMetrics::horizontalAdvance()`
- **风险**：低 — CodeEditor 行号基础设施已存在，主要是可见性配置和主题适配

### 1.2 DiffMinimap 绘制逻辑修复（🔴 #2）

**问题根因**：
1. `paintEvent` 中先调用 `QWidget::paintEvent(event)` — 这会用样式表背景色（或默认灰色）完整填充整个控件，之后的 diff 色带绘制在上面无法区分与背景；更关键的是如果控件有 border/padding，默认绘制可能覆盖预期外观。
2. 逐行 `fillRect` — 当 diff 有 10000 行时，每帧调用 10000 次 `fillRect`，重绘开销极大。

**技术方案**：

- **文件**：`src/app/widgets/DiffMinimapWidget.cpp`
- **方案**：
  1. **移除** `QWidget::paintEvent(event)` 调用，改为手动绘制背景：
     ```cpp
     painter.fillRect(rect(), palette().color(QPalette::Base)); // 遵循主题
     ```
  2. **合并相邻同色行**：在绘制前先将 `m_diffData` 扫描为连续同色段（run-length encoding），只对每个段调用一次 `fillRect`
  3. **缓存绘制结果**：添加 `QPixmap m_cache` 成员，仅在 `setDiffData()` 时重新生成缓存位图，`paintEvent()` 中直接 `drawPixmap`
  4. 添加 `QPen` 为 `Qt::NoPen` 已存在，保持
  5. 将硬编码的颜色（`Qt::green`, `Qt::red` 等）改为可配置的成员变量，或使用项目 QSS 中的 CSS 变量
- **Qt API**：`QPainter::fillRect()`, `QPixmap`, `QPainter::drawPixmap()`, `QPalette::base()`
- **风险**：低 — 逻辑简单，缓存位图显著提升性能

### 1.3 大文件 diff UI 卡顿优化（🔴 #3）

**问题根因**：
`renderDiffs()` 中 `m_leftEditor->setPlainText(leftLines.join('\n'))` 将全部文本一次性传入。`QPlainTextEdit` 需要对所有行进行布局计算（QTextLayout），大文件（10000+ 行）会在主线程中阻塞数百毫秒，造成 UI 冻结。

**技术方案**：

- **文件**：`src/app/pages/FileComparePage.cpp`
- **方案**：分块渐进式渲染（Lazy Rendering）
  1. 添加 `QTimer m_renderTimer` 和 `int m_renderChunkIndex` 成员
  2. 修改 `renderDiffs()`：不再一次性 `setPlainText`，而是
     - 先调用 `m_leftEditor->document()->clear()` 清空文档
     - 将 `renderLines` 列表保存为成员 `m_pendingRenderLines`
     - 启动增量渲染：设置 `m_renderChunkIndex = 0`，调用 `m_renderTimer->singleShot(0, this, &FileComparePage::renderNextChunk)`
  3. 实现 `renderNextChunk()`：
     - 每帧追加 `kChunkSize = 200` 行
     - 使用 `QTextCursor` 在文档末尾插入文本块：
       ```cpp
       QTextCursor cursor(m_leftEditor->document());
       cursor.movePosition(QTextCursor::End);
       for (int i = 0; i < kChunkSize && idx < total; ++i, ++idx) {
           cursor.insertBlock();
           cursor.insertText(pendingLine);
       }
       ```
     - 当所有行渲染完成后再设置 ExtraSelections（差异高亮）
  4. 显示进度指示：在大文件加载时在 summaryLabel 显示 "正在渲染... X%"
  5. 添加 `QApplication::processEvents()` 在每块之间保持 UI 响应（或使用 `QTimer::singleShot(0, ...)` 自然让出事件循环）
- **替代方案**（更轻量）：如果不想改动太大，可以用 `QPlainTextEdit::setMaximumBlockCount(0)` 并设置 `setUndoRedoEnabled(false)` + `document()->setDefaultFont()` 等预处理来加速 `setPlainText`。但这只是缓解而非根治。
- **Qt API**：`QTextCursor::insertBlock()`, `QTextCursor::insertText()`, `QTimer::singleShot()`
- **风险**：中 — 需要仔细处理 ExtraSelections 的延迟设置时机，确保光标不跳变

### 1.4 滚动联动改为同步滚动（🔴 #4）

**问题根因**：
当前实现是"硬绑定"：
```cpp
connect(m_leftEditor->verticalScrollBar(),  &QScrollBar::valueChanged,
        m_rightEditor->verticalScrollBar(), &QScrollBar::setValue);
```
问题：
1. **无限递归风险**：A 的 `valueChanged` 设 B 的 value，B 的 `valueChanged` 又设 A 的 value。虽然 Qt 的 `valueChanged` 仅在值真正变化时发射，但若两个编辑器行数不同（例如左 500 行 / 右 520 行），一侧可能被强制滚到超出范围的 value，导致位置跳变。
2. **比率错误**：如果两个文档行数不同，应该按百分比比例同步，而非按绝对 value。

**技术方案**：

- **文件**：`src/app/pages/FileComparePage.cpp`
- **方案**：实现"按比例同步滚动"
  1. 移除现有的 4 个 `QScrollBar::valueChanged` → `setValue` 直连
  2. 添加 `bool m_syncingScroll = false` 互斥锁标志
  3. 实现 slot `syncScrollBar(int value, QScrollBar *source, QScrollBar *target)`：
     ```cpp
     void FileComparePage::syncScrollBar(int /*value*/, QScrollBar *source, QScrollBar *target) {
         if (m_syncingScroll) return;
         m_syncingScroll = true;
         const int srcMax = source->maximum();
         const int tgtMax = target->maximum();
         if (srcMax > 0 && tgtMax > 0) {
             double ratio = static_cast<double>(source->value()) / srcMax;
             target->setValue(static_cast<int>(ratio * tgtMax));
         }
         m_syncingScroll = false;
     }
     ```
  4. 连接信号：
     ```cpp
     connect(m_leftEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int v) {
         syncScrollBar(v, m_leftEditor->verticalScrollBar(), m_rightEditor->verticalScrollBar());
     });
     // 右侧同理（水平滚动同理）
     ```
  5. 同样的逻辑应用于 `horizontalScrollBar()`
- **Qt API**：`QScrollBar::maximum()`, `QScrollBar::value()`, `QScrollBar::setValue()`
- **风险**：低 — 纯逻辑改动，不涉及 UI 布局

---

## 阶段二：算法增强 — 准确性与编码

> **目标**：提升 diff 算法精度、修复未使用配置、支持多编码文件、增强二进制比较。
> **完成标准**：配置项生效、Modified 配对更准确、GBK/UTF-16 文件不乱码、二进制差异字节可见。
> **预计总工时**：~14h

### 2.1 maxExactLines 清理与生效（🟢 #14）

**问题根因**：
`FileCompareOptions::maxExactLines` 定义了（默认 5000），但 `FileCompareEngine.cpp` 中硬编码了 `kExactMatrixCellLimit = 40000` 的匿名命名空间常量，从未读取 `options.maxExactLines`。

**技术方案**：

- **文件**：`src/core/FileCompareEngine.cpp`
- **方案**：
  1. 移除匿名命名空间中的 `kExactMatrixCellLimit`
  2. 修改 `buildLargeDiff()` 中的判断条件（第 201 行）：
     从 `static_cast<qint64>(aLen) * static_cast<qint64>(bLen) <= kExactMatrixCellLimit`
     改为使用传入的 `maxExactLines` 参数
  3. 在 `FileCompareEngine::compare()` 中将 `options.maxExactLines` 传入 `buildLargeDiff()`
  4. 修改 `buildLargeDiff()` 签名，增加 `int maxExactLines` 参数
- **Qt API**：无需特殊 API
- **测试**：更新 `tests/file_compare_engine_tests.cpp` 中添加测试用例，验证 `maxExactLines` 设小值时走大文件分治路径
- **风险**：低

### 2.2 coalesceChangedRuns 配对算法改进（🟢 #15）

**问题根因**：
当前 `coalesceChangedRuns` 使用"按顺序贪心配对"：
```cpp
const int paired = std::min(removed.size(), added.size());
for (int i = 0; i < paired; ++i) {
    result.append({removed.at(i), added.at(i), DiffLine::Modified});
}
```
如果一段变更有 "删除 A / 删除 B / 新增 C / 新增 D"，顺序配对会把 A↔C 和 B↔D 配对为 Modified，但实际可能是 A↔D / B↔C 更合理（如果 A 和 D 内容更相似）。

**技术方案**：

- **文件**：`src/core/FileCompareEngine.cpp`
- **方案**：改用基于相似度的 LCS 配对
  1. 对 `removed` 和 `added` 列表计算相似度矩阵（使用 `changedRange` 的 prefix+suffix 匹配度，或简单的 Levenshtein 归一化相似度）
  2. 使用匈牙利算法或贪心最大权匹配（`O(n²)` 在小段时可行）：
     - 优先配对相似度 > 阈值（如 0.3）的行
     - 剩余未配对的行保持原 Add/Remove 状态
  3. 实现辅助函数 `double lineSimilarity(const QString &a, const QString &b)`：
     - 基于 `changedRange()` 中 prefix + suffix 长度占总长比例计算
     ```cpp
     double lineSimilarity(const QString &a, const QString &b) {
         const int len = std::max(1, std::max(a.size(), b.size()));
         const int commonLen = std::min(a.size(), b.size());
         int prefix = 0, suffix = 0;
         while (prefix < commonLen && a[prefix] == b[prefix]) ++prefix;
         while (suffix < commonLen - prefix && a[a.size()-1-suffix] == b[b.size()-1-suffix]) ++suffix;
         return static_cast<double>(prefix + suffix) / len;
     }
     ```
  4. 贪心最大权匹配：构建 `removed.size() × added.size()` 矩阵，每次选最高相似度配对，移除已用行列，直到相似度 < 0.2
- **Qt API**：无需特殊 API
- **测试**：添加测试用例验证"删除 A / 删除 B / 新增 C(=B) / 新增 D(=A)"能正确配对
- **风险**：低 — 小段内计算量可以忽略

### 2.3 文件编码检测（🔵 #20）

**问题根因**：
`loadFileContent()` 中文本模式直接用 `QTextStream` 的默认编码（系统 locale），对 GBK/GB2312/UTF-16BE/UTF-16LE 文件可能乱码。二进制检测仅通过扩展名和 NUL 字节判断。

**技术方案**：

- **文件**：`src/core/FileCompareEngine.h` / `FileCompareEngine.cpp`（或新建 `src/core/services/FileEncodingDetector.h`）
- **方案**：
  1. 在 `FileCompareEngine` 或新建工具类中实现 `detectEncoding(const QByteArray &data)`：
     - 检查 BOM：`EF BB BF` → UTF-8, `FF FE` → UTF-16LE, `FE FF` → UTF-16BE
     - 无 BOM 时，使用启发式检测：
       - UTF-8 合法性检查（multi-byte 序列）
       - GBK/GB2312 检测（双字节高位置 1 模式）
       - 中文 Windows 和嵌入式行业 GBK 文件常见
     - 兜底：系统 locale 编码
  2. 返回 `QString` 编码名称（如 `"UTF-8"`, `"GBK"`, `"UTF-16LE"`），存入 `LoadedFile` 结构体
  3. 修改 `LoadFile`：
     - 先用 `QFile::readAll()` 读取原始字节
     - 调用 `detectEncoding()` 检测编码
     - 使用 `QTextCodec::codecForName()` （Qt5）或 `QStringDecoder`（Qt6）解码
  4. 添加 `m_encodingCombo`（QComboBox）到工具栏，允许用户手动覆盖编码选择（自动检测 / UTF-8 / GBK / UTF-16LE / UTF-16BE / Latin-1）
  5. `LoadedFile` 结构体增加 `QString encoding` 字段
  6. `FileComparePage` 增加 `QComboBox *m_encodingCombo` 成员
- **Qt API**：`QStringDecoder` (Qt6), `QStringConverter::encodingForName()`, `QByteArrayView`
- **风险**：中 — 编码检测不是 100% 可靠，需提供手动覆盖选项

### 2.4 二进制比较字节级差异高亮（🟡 #6）

**问题根因**：
二进制模式生成的是 hex dump 文本行（`00000000: 48 65 6C 6C ...`），但 `FileCompareEngine::compare()` 把整个 hex dump 行当作一个字符串比较。当一行中只有一个字节不同时（如 `FF` vs `FE`），整行被标记为 Modified，但用户看不出具体哪个字节变了。

**技术方案**：

- **文件**：`src/core/FileCompareEngine.cpp`（引擎侧）+ `src/app/pages/FileComparePage.cpp`（渲染侧）
- **方案**：两级 diff — 行级 + 字节级
  1. 在 `DiffLine` 结构体中增加可选的字节级差异信息：
     ```cpp
     struct ByteDiff {
         int offset;   // 在 hex dump 行中的字符偏移
         int length;   // 变化的字符数
     };
     QVector<ByteDiff> byteDiffs; // DiffLine 新增字段，仅 Modified 行使用
     ```
  2. 在 `FileCompareEngine::compare()` 返回后，对每个 `Modified` 行调用新增函数 `computeByteDiffs(const QString &lineA, const QString &lineB)`：
     - 逐字节比较 hex dump 的 hex 部分（偏移 10-57）
     - 发现不同字节时，计算在 hex dump 字符串中的字符位置（每个字节占 3 个字符：`XX `）
     - 生成 `ByteDiff` 列表
  3. 在 `renderDiffs()` 中，对 Modified 行额外应用字符级 `ExtraSelection`，用更深的高亮色（如 `#E8A820`）标记变化的字节位置
  4. 实现 `computeByteDiffs()`：
     ```cpp
     QVector<ByteDiff> computeByteDiffs(const QString &hexLineA, const QString &hexLineB) {
         // hex dump 格式: "00000000: FF FE AB ... | .text."
         // 从偏移 10 开始比较 hex 部分
         QVector<ByteDiff> diffs;
         int start = -1;
         for (int i = 10; i < std::min(hexLineA.size(), hexLineB.size()); ++i) {
             if (hexLineA[i] != hexLineB[i]) {
                 if (start < 0) start = i;
             } else if (start >= 0) {
                 diffs.append({start, i - start});
                 start = -1;
             }
         }
         if (start >= 0) diffs.append({start, static_cast<int>(std::min(hexLineA.size(), hexLineB.size())) - start});
         return diffs;
     }
     ```
- **Qt API**：`QTextEdit::ExtraSelection` 已在使用，扩展即可
- **风险**：低 — 仅影响渲染层，不改变 diff 算法核心

---

## 阶段三：交互提升 — 导航与操作

> **目标**：完善用户导航能力、修复交互细节、增加拖拽和快捷键。
> **完成标准**：可输入行号跳转、交换保留位置、预览模式状态清晰、支持拖拽打开、常用快捷键可用。
> **预计总工时**：~17h

### 3.1 "转到行"功能（🟡 #5）

**问题根因**：
没有行号输入框或跳转能力，用户只能手动滚动或依赖 diff 导航按钮。

**技术方案**：

- **文件**：`src/app/pages/FileComparePage.h` / `.cpp`
- **方案**：
  1. 在工具栏 `optionLayout` 中添加行号跳转组件：
     ```cpp
     auto *gotoLayout = new QHBoxLayout();
     auto *gotoLabel = new QLabel(tr("转到行:"), this);
     m_gotoLineEdit = new QLineEdit(this);
     m_gotoLineEdit->setPlaceholderText(tr("行号"));
     m_gotoLineEdit->setMaximumWidth(80);
     m_gotoLineEdit->setValidator(new QIntValidator(1, 999999, this));
     auto *gotoBtn = new QPushButton(tr("跳转"), this);
     ```
  2. 连接 `gotoBtn::clicked` 和 `m_gotoLineEdit::returnPressed` 到 slot `gotoLine()`：
     ```cpp
     void FileComparePage::gotoLine() {
         bool ok;
         int lineNo = m_gotoLineEdit->text().toInt(&ok);
         if (!ok || lineNo < 1) return;
         int lineIdx = lineNo - 1; // 转为 0-based
         QTextBlock blockL = m_leftEditor->document()->findBlockByNumber(lineIdx);
         QTextBlock blockR = m_rightEditor->document()->findBlockByNumber(lineIdx);
         if (blockL.isValid()) {
             m_leftEditor->setTextCursor(QTextCursor(blockL));
             m_leftEditor->centerCursor();
         }
         if (blockR.isValid()) {
             m_rightEditor->setTextCursor(QTextCursor(blockR));
             m_rightEditor->centerCursor();
         }
         m_gotoLineEdit->selectAll();
     }
     ```
  3. 使用 `QShortcut` 绑定 `Ctrl+G` 激活行号输入框：
     ```cpp
     auto *gotoShortcut = new QShortcut(QKeySequence("Ctrl+G"), this);
     connect(gotoShortcut, &QShortcut::activated, [this]() {
         m_gotoLineEdit->setFocus();
         m_gotoLineEdit->selectAll();
     });
     ```
- **新增成员**：`QLineEdit *m_gotoLineEdit`
- **Qt API**：`QIntValidator`, `QShortcut::activated`, `QTextBlock::findBlockByNumber()`
- **风险**：低

### 3.2 交换后保留差异导航位置（🟡 #7）

**问题根因**：
`swapFiles()` 直接调用 `compareFiles()`，后者将 `m_currentDiffIndex` 重置为 -1。

**技术方案**：

- **文件**：`src/app/pages/FileComparePage.cpp`
- **方案**：
  1. 修改 `swapFiles()`：保存当前差异块索引和总块数
     ```cpp
     void FileComparePage::swapFiles() {
         const int savedIndex = m_currentDiffIndex;
         const int totalBlocks = m_diffBlockIndexes.size();
         std::swap(m_leftFile, m_rightFile);
         updateFileLabels();
         compareFiles();
         // 尝试恢复位置 — 交换后差异块索引可能变化，尽量接近
         if (savedIndex >= 0 && totalBlocks > 0 && !m_diffBlockIndexes.isEmpty()) {
             m_currentDiffIndex = std::min(savedIndex, m_diffBlockIndexes.size() - 1);
             navigateDiff(0); // 0 delta = 原地导航以滚动到当前位置
         }
     }
     ```
  2. 或者更精确：不调用 `compareFiles()` 重新 diff，而是直接交换 `m_allDiffs` 中 textA/textB 并反转 status：
     ```cpp
     void FileComparePage::swapFiles() {
         std::swap(m_leftFile, m_rightFile);
         // 直接翻转现有 diff 结果，避免重新计算
         for (auto &diff : m_allDiffs) {
             std::swap(diff.textA, diff.textB);
             if (diff.status == DiffLine::Added) diff.status = DiffLine::Removed;
             else if (diff.status == DiffLine::Removed) diff.status = DiffLine::Added;
         }
         updateFileLabels();
         renderDiffs(); // 保留 m_currentDiffIndex
         if (m_currentDiffIndex >= 0) navigateDiff(0);
     }
     ```
  3. 选择方案 2（更高效且精确），因为它避免了重新 diff
- **风险**：低

### 3.3 单文件预览模式提示增强（🟡 #8）

**问题根因**：
当只加载了左侧或右侧文件时，虽然状态卡片显示"等待另一侧文件"，但编辑器区域缺少明显的视觉标识告知用户当前是预览状态。

**技术方案**：

- **文件**：`src/app/pages/FileComparePage.cpp`
- **方案**：
  1. 在 `renderDiffs()` 中单文件预览分支，设置编辑器顶部添加信息栏（类似浏览器 "info bar"）：
     ```cpp
     // 在 m_leftEditor / m_rightEditor 上方或者使用 QPlainTextEdit 的
     // document()->setDefaultStyleSheet() 方式不可行，改用 QLabel 浮层
     ```
  2. 更简单的方案：在 `m_contentStack` 中增加一个预览模式指示页，或在 summaryFrame 中用醒目颜色标注：
     ```cpp
     m_summaryLabel->setStyleSheet("color: #E8A820; font-weight: bold;");
     m_summaryLabel->setText(tr("⚠ 预览模式 — 已加载 %1，请加载另一侧文件后进行完整比较")
         .arg(m_leftFile.loaded ? tr("左侧文件") : tr("右侧文件")));
     ```
  3. 此外，在编辑器背景添加细微的水印条纹或半透明提示层（可选，增加视觉区分度）
- **风险**：低

### 3.4 文件拖拽支持（🟡 #9）

**问题根因**：
用户必须通过"打开左侧/右侧文件"按钮选择文件，不支持拖拽到编辑器区域。

**技术方案**：

- **文件**：`src/app/pages/FileComparePage.h` / `.cpp`
- **方案**：
  1. 重写 FileComparePage 或编辑器的拖放事件：
     ```cpp
     // 在 setupUi() 中
     setAcceptDrops(true);
     m_leftEditor->setAcceptDrops(true);
     m_rightEditor->setAcceptDrops(true);
     ```
  2. 重写 `dragEnterEvent()`, `dragMoveEvent()`, `dropEvent()`：
     ```cpp
     void FileComparePage::dragEnterEvent(QDragEnterEvent *event) {
         if (event->mimeData()->hasUrls()) {
             event->acceptProposedAction();
         }
     }
     void FileComparePage::dropEvent(QDropEvent *event) {
         const QList<QUrl> urls = event->mimeData()->urls();
         if (urls.isEmpty()) return;
         // 第一个文件 → 左侧，第二个文件（如有）→ 右侧
         loadFileFromUrl(urls.at(0), true);
         if (urls.size() > 1) {
             loadFileFromUrl(urls.at(1), false);
         }
     }
     ```
  3. 提取公共方法 `loadFileFromUrl(const QUrl &url, bool leftSide)`
  4. 对于已有文件的侧，拖入新文件替换；对于空侧优先填充
  5. 拖拽区域高亮：`dragEnterEvent` 中设置编辑器边框高亮样式
  6. 对 `m_leftEditor` 和 `m_rightEditor` 安装事件过滤器，将拖放事件冒泡到 `FileComparePage` 统一处理
- **Qt API**：`QDragEnterEvent`, `QDropEvent`, `QMimeData::hasUrls()`, `QMimeData::urls()`
- **风险**：低 — Qt 拖放体系成熟

### 3.5 键盘快捷键（🟢 #17）

**问题根因**：
所有操作依赖鼠标点击按钮，缺少常用 IDE 快捷键。

**技术方案**：

- **文件**：`src/app/pages/FileComparePage.cpp`
- **方案**：在 `setupUi()` 末尾使用 `QShortcut` 注册以下快捷键：

| 快捷键 | 功能 | 实现 |
|--------|------|------|
| `Ctrl+O` | 打开左侧文件 | `connect(shortcut, &QShortcut::activated, [this]{ openFile(true); })` |
| `Ctrl+Shift+O` | 打开右侧文件 | `openFile(false)` |
| `Ctrl+R` 或 `F5` | 重新比较 | `compareFiles()` |
| `F3` / `Shift+F3` | 下一处 / 上一处差异 | `navigateDiff(1)` / `navigateDiff(-1)` |
| `Ctrl+G` | 转到行（联动 #5） | 聚焦 m_gotoLineEdit |
| `Ctrl+F` | 搜索（预留） | 如实现搜索则绑定 |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | 在差异块间切换 | 同 F3 / Shift+F3 |
| `Ctrl+D` | 切换"只看差异" | `m_showOnlyDiffsCheckBox->toggle()` |
| `Ctrl+S` | 交换左右文件 | `swapFiles()` |
| `Escape` | 清空比较 | `clearComparison()` |

  ```cpp
  // 示例
  auto *nextDiffShortcut = new QShortcut(QKeySequence("F3"), this);
  connect(nextDiffShortcut, &QShortcut::activated, [this]() { navigateDiff(1); });
  ```
- **Qt API**：`QShortcut`, `QKeySequence`
- **风险**：低 — 注意快捷键冲突（`Ctrl+O` 可能与某些系统快捷键冲突）

---

## 阶段四：功能扩展 — 生产力

> **目标**：增加差异合并/编辑、折叠展开、图标、状态卡片美化、导出功能。
> **完成标准**：可接受/拒绝差异、折叠行可点击展开、工具栏有图标、卡片颜色有区分、支持导出 patch/HTML。
> **预计总工时**：~26h

### 4.1 差异合并/编辑功能（🟡 #10）

**问题根因**：
比较页仅展示差异，用户无法在编辑器内接受、拒绝或编辑变更。

**技术方案**：

- **文件**：`src/app/pages/FileComparePage.h` / `.cpp`
- **方案**：添加差异块操作按钮和编辑能力
  1. 在工具栏增加 3 个按钮（仅在选中差异块时激活）：
     - "接受左侧" — 将右侧行替换为左侧行
     - "接受右侧" — 将左侧行替换为右侧行
     - "编辑" — 允许手动编辑
  2. 实现编辑模式：
     - 添加 `m_editMode` 标志
     - 在编辑模式下，将左右编辑器设为可读写
     - 用户可直接在两侧编辑器中修改文本
     - 提供"应用修改"按钮重新 diff
  3. 实现"接受一侧"操作：
     ```cpp
     void FileComparePage::acceptLeft() {
         // 在当前差异块中，将右侧文本替换为左侧文本
         const auto &diff = m_allDiffs.at(currentDiffLineIndex);
         // 修改 m_rightFile.lines 中对应行
         // 重新渲染
     }
     ```
  4. 在差异导航到某块时，自动定位并高亮块内所有行
  5. 添加"撤销最近一次合并"能力
  6. 操作后自动更新 diff 计数和 minimap
- **Qt API**：`QTextCursor`, `QTextDocument::undo()`
- **风险**：中 — 需要管理编辑状态和文件同步，建议先做简单的"接受左侧/右侧"功能

### 4.2 "只看差异"折叠行可展开（🟢 #16）

**问题根因**：
当前 `renderDiffs()` 用 `collapsedBlockText()` 生成占位行，但用户点击占位行无反应。

**技术方案**：

- **文件**：`src/app/pages/FileComparePage.h` / `.cpp`
- **方案**：
  1. 记录折叠行在 `renderLines` 中的索引及其对应的隐藏范围
  2. 使占位行可交互：（方案 A）在占位行文本中加入提示 "点击展开"
  3. 连接编辑器点击事件：安装事件过滤器或重写 `mouseReleaseEvent`
  4. 更好的方案：在占位行旁边添加小型 `QPushButton`（通过 `QTextObjectInterface`）或直接使用 `QTextEdit` 的 `anchorClicked` 信号（将折叠行文本设为富文本链接）
  5. 使用 `QTextEdit` 的富文本能力：
     ```cpp
     QString placeholder = QString("<a href='expand:%1-%2' style='color:#2F7FB5;'>▶ %3</a>")
         .arg(prevEnd).arg(nextStart).arg(tr("展开 %1 行相同内容").arg(hiddenLines));
     ```
     在编辑器 `setReadOnly(false)` 的情况下，连接 `anchorClicked(QUrl)` 信号来展开
  6. 实现展开逻辑：重新调用 `renderDiffs()` 但这次不折叠该块
  7. 添加成员 `QSet<int> m_expandedBlocks` 记录用户展开的块索引
  8. 修改 `renderDiffs()` — 检查 `m_expandedBlocks` 跳过折叠
- **Qt API**：`QTextEdit::anchorAt()`, `QTextEdit::anchorClicked(QUrl)`, 富文本 `<a href>`
- **风险**：中 — 需要处理富文本与只读模式的兼容问题

### 4.3 工具栏按钮图标化（🟢 #13）

**问题根因**：
所有按钮纯文字，占据大量水平空间，视觉不直观。

**技术方案**：

- **文件**：`src/app/pages/FileComparePage.cpp` + `src/resources/resources.qrc`
- **方案**：
  1. 使用 Qt 内置图标（`QStyle::standardIcon`）或 Unicode 符号作为图标：
     ```cpp
     // 使用 QStyle 标准图标
     m_btnOpenLeft->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
     
     // 或使用 Unicode 符号（轻量、无需额外资源文件）
     m_btnPrevDiff->setText(tr("◀ 上一处"));
     m_btnNextDiff->setText(tr("下一处 ▶"));
     m_btnCompare->setText(tr("⇔ 比较"));
     m_btnSwap->setText(tr("⇄ 交换"));
     m_btnClear->setText(tr("✕ 清空"));
     ```
  2. 更好的方案：创建 SVG 图标资源
     - 新建 `src/resources/icons/` 目录
     - 使用简单 SVG 图标（16×16, 24×24）
     - 或者生成 QIcon 通过 `QIcon::fromTheme()` 使用系统主题图标
  3. 设置按钮样式为 `toolButtonStyle(Qt::ToolButtonTextBesideIcon)` 或在 QSS 中定义
  4. 对于只显示图标的按钮，使用 `QToolButton` 代替 `QPushButton`：
     ```cpp
     auto *btn = new QToolButton(this);
     btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
     btn->setIcon(QIcon(":/icons/compare.svg"));
     btn->setText(tr("比较"));
     ```
  5. 更新 `resources.qrc` 添加图标资源
- **新增文件**：图标 SVG 文件或 `icons.qrc`
- **Qt API**：`QStyle::standardIcon()`, `QIcon`, `QToolButton`
- **风险**：低 — 纯视觉改动

### 4.4 状态卡片样式增强（🟢 #12）

**问题根因**：
三个状态卡片在无文件时都显示同一种灰色（`#5F7890`），缺乏视觉区分。

**技术方案**：

- **文件**：`src/app/widgets/IndustrialStatusCard.h` / `.cpp` + `src/app/pages/FileComparePage.cpp`
- **方案**：
  1. 为 `IndustrialStatusCard` 添加不同状态预设：
     ```cpp
     void IndustrialStatusCard::setState(State state); // Idle, Loading, Success, Warning, Error
     ```
  2. 每个状态对应一种颜色主题：
     - Idle（未选择）：浅灰蓝 `#7A8FA0`
     - Loading（加载中）：蓝色 `#2F7FB5`
     - Success（无差异）：绿色 `#2C8C5A`
     - Warning（有差异）：琥珀色 `#B8801E`
     - Error（加载失败）：红色 `#C0392B`
  3. 在 `FileComparePage::updateStatusCards()` 中使用这些预设
  4. 为卡片添加微妙的渐变背景或左侧色条（通过 QSS 或 paintEvent）
  5. 修改 `IndustrialStatusCard` 构造函数或 setStatus 支持更多参数
  6. 可选：卡片增加小图标（✓ / ⚠ / ✗）
- **风险**：低

### 4.5 导出差异功能（🟢 #18）

**问题根因**：
无法将比较结果导出为外部格式，不利于代码审查、文档记录或补丁应用。

**技术方案**：

- **文件**：`src/core/FileCompareEngine.h` + `src/app/pages/FileComparePage.h` / `.cpp`
- **方案**：支持三种导出格式
  1. 在引擎层添加导出函数：
     ```cpp
     // FileCompareEngine.h
     static QString exportUnifiedDiff(const QStringList &linesA, const QStringList &linesB,
                                      const QString &fileA, const QString &fileB,
                                      const QList<DiffLine> &diffs);
     static QString exportHtmlDiff(const QStringList &linesA, const QStringList &linesB,
                                   const QList<DiffLine> &diffs);
     static QString exportTextSummary(const QList<DiffLine> &diffs);
     ```
  2. **Unified Diff 格式**（标准 patch 格式）：
     ```cpp
     // 生成标准 unified diff header
     // --- a/fileA
     // +++ b/fileB
     // @@ -l,s +l,s @@ context
     // -removed line
     // +added line
     //  context line
     ```
  3. **HTML 格式**：生成带颜色的 HTML 表格，适合在浏览器查看或嵌入报告
  4. **纯文本摘要**：差异统计 + 变更块列表
  5. 在 `FileComparePage` 添加"导出"按钮和格式选择：
     ```cpp
     QMenu *exportMenu = new QMenu(this);
     exportMenu->addAction(tr("导出 Unified Diff (.patch)"), ...);
     exportMenu->addAction(tr("导出 HTML 报告"), ...);
     exportMenu->addAction(tr("导出文本摘要"), ...);
     m_btnExport->setMenu(exportMenu);
     ```
  6. 使用 `QFileDialog::getSaveFileName()` 选择保存路径
- **Qt API**：`QFileDialog::getSaveFileName()`, `QMenu`, `QTextStream`
- **风险**：中 — Unified diff 格式有标准规范，需确保生成的 patch 可被 `git apply` 等工具识别

---

## 阶段五：架构优化 — 长期改进

> **目标**：重构 diff 视图架构，增加最近记录，支持字体调整。
> **完成标准**：可选的优化 diff 渲染器、最近比较记录可持久化、字体/字号可调。
> **预计总工时**：~23h

### 5.1 自定义 Diff 视图（🔵 #19）

**问题根因**：
`QTextEdit`/`QPlainTextEdit` 作为 diff 查看器存在固有局限：
- 文本编辑器的行号、光标、选择等概念对只读 diff 视图是多余的
- `ExtraSelections` 性能在大文件时下降
- 不支持虚拟滚动（所有行都在内存中）
- 两侧独立滚动条难以真正同步

**技术方案**：

- **文件**：新建 `src/app/widgets/DiffView.h` / `DiffView.cpp`
- **方案**：基于 `QAbstractScrollArea` 实现专用 diff 渲染器
  1. 设计 `DiffView` 类：
     ```cpp
     class DiffView : public QAbstractScrollArea {
         Q_OBJECT
     public:
         void setDiffData(const QList<DiffLine> &diffs);
         void setLineNumbersVisible(bool visible);
         void goToLine(int lineIndex);
         void setFont(const QFont &font);
     signals:
         void lineClicked(int lineIndex);
     protected:
         void paintEvent(QPaintEvent *event) override;
         void resizeEvent(QResizeEvent *event) override;
         void mousePressEvent(QMouseEvent *event) override;
         void wheelEvent(QWheelEvent *event) override;
     };
     ```
  2. 核心优势：
     - **虚拟渲染**：只绘制可见行（`firstVisibleLine` ~ `lastVisibleLine`）
     - **统一坐标系**：左右面板共享同一逻辑行索引
     - **性能**：无 `QTextDocument` 开销，50万行文件也能流畅滚动
     - **内置行号**：行号作为左侧 gutter 绘制，无需额外组件
     - **差异高亮**：直接在 `paintEvent` 中用 `QPainter::fillRect` 绘制行背景
  3. 渲染逻辑：
     ```cpp
     void DiffView::paintEvent(QPaintEvent *event) {
         QPainter painter(viewport());
         painter.fillRect(event->rect(), m_backgroundColor);
         
         int y = -m_scrollOffset;
         int lineHeight = m_fontMetrics.lineSpacing() + m_lineSpacing;
         int firstVisible = m_scrollOffset / lineHeight;
         int lastVisible = (m_scrollOffset + viewport()->height()) / lineHeight + 1;
         
         int gutterWidth = m_showLineNumbers ? m_gutterWidth : 0;
         int leftWidth = (viewport()->width() - gutterWidth) / 2;
         
         for (int i = firstVisible; i <= lastVisible && i < m_diffs.size(); ++i) {
             // 绘制行号 gutter
             if (m_showLineNumbers) { drawLineNumber(painter, i, y, gutterWidth); }
             // 绘制左侧文本
             drawLineText(painter, m_diffs[i].textA, gutterWidth, y, leftWidth, m_diffs[i].status);
             // 绘制右侧文本
             drawLineText(painter, m_diffs[i].textB, gutterWidth + leftWidth, y, leftWidth, m_diffs[i].status);
             // 差异行背景
             drawLineBackground(painter, gutterWidth, y, viewport()->width() - gutterWidth, m_diffs[i].status);
             y += lineHeight;
         }
     }
     ```
  4. 在 `FileComparePage` 中条件编译或用 `QStackedWidget` 切换新旧视图：
     ```cpp
     #ifdef EST_USE_DIFFVIEW
         m_diffView = new DiffView(this);
     #else
         // 使用现有 CodeEditor 方式
     #endif
     ```
  5. 逐步迁移：Phase 5 实现 `DiffView`，作为实验性功能通过设置开启
- **新增文件**：`src/app/widgets/DiffView.h`, `src/app/widgets/DiffView.cpp`
- **Qt API**：`QAbstractScrollArea`, `QPainter`, `QFontMetrics`, `QScrollBar`
- **风险**：高 — 替代核心渲染组件，需充分测试。建议先作为可选特性实现，不替换现有方案

### 5.2 最近比较记录（🔵 #21）

**问题根因**：
没有保存"最近比较的文件对"，用户每次都要重新选择。

**技术方案**：

- **文件**：`src/core/services/RecentRecordManager.h` / `.cpp`（扩展现有系统）+ `src/app/pages/FileComparePage.cpp`
- **方案**：
  1. 在 `RecentRecordManager` 中添加 `addComparePair()` 和 `recentComparePairs()` 方法：
     ```cpp
     void addComparePair(const QString &leftPath, const QString &rightPath,
                        qint64 leftSize, qint64 rightSize);
     QVariantList recentComparePairs() const;
     ```
  2. 存储 key 为 `"comparePairs"`，记录包含：
     ```json
     {
       "leftPath": "...",
       "rightPath": "...",
       "leftName": "...",
       "rightName": "...",
       "leftSize": 1234,
       "rightSize": 5678,
       "timestamp": "2026-05-07 14:30"
     }
     ```
  3. 在 `compareFiles()` 成功后调用 `RecentRecordManager::addComparePair()`
  4. 在 `FileComparePage` 添加"最近比较"下拉菜单或列表：
     ```cpp
     QMenu *recentMenu = new QMenu(tr("最近比较"), this);
     m_btnRecent->setMenu(recentMenu);
     // 动态填充最近 10 条记录
     for (const auto &record : recentPairs) {
         QString label = QString("%1 ↔ %2").arg(leftName, rightName);
         recentMenu->addAction(label, [this, record]() {
             // 自动加载左侧和右侧文件
             loadFileByPath(record.leftPath, true);
             loadFileByPath(record.rightPath, false);
         });
     }
     ```
  5. 添加 `QPushButton *m_btnRecent` 到工具栏
  6. 在 `clearComparison()` 时不清理历史记录（记录是持久化的）

- **Qt API**：`QMenu`, `QJsonDocument`（已有）
- **风险**：低 — 扩展现有 RecentRecordManager 体系

### 5.3 编辑器字体调整（🟢 #11）

**问题根因**：
编辑器字体固定，hex dump 长行（70+ 字符）在小屏幕上可能截断或字体不合适。

**技术方案**：

- **文件**：`src/app/widgets/CodeEditor.h` / `.cpp` + `src/app/pages/FileComparePage.cpp`
- **方案**：
  1. 在 `CodeEditor` 中添加方法：
     ```cpp
     void setEditorFont(const QFont &font);
     QFont editorFont() const;
     void zoomIn();  // Ctrl+=
     void zoomOut(); // Ctrl+-
     void zoomReset(); // Ctrl+0
     ```
  2. 实现 `zoomIn/Out`：
     ```cpp
     void CodeEditor::zoomIn() {
         QFont f = font();
         f.setPointSize(f.pointSize() + 1);
         setEditorFont(f);
     }
     void CodeEditor::setEditorFont(const QFont &font) {
         setFont(font);
         updateLineNumberAreaWidth(0); // 重新计算行号区域宽度
     }
     ```
  3. 在 `FileComparePage` 工具栏添加字体控制：
     ```cpp
     auto *zoomOutBtn = new QPushButton("A-", this);
     auto *zoomInBtn = new QPushButton("A+", this);
     auto *fontCombo = new QFontComboBox(this); // 字体选择
     auto *sizeSpin = new QSpinBox(this); // 字号 6-48
     sizeSpin->setValue(font().pointSize());
     ```
  4. 字体更改应用到两侧编辑器和 minimap
  5. 绑定 `Ctrl+=` / `Ctrl+-` / `Ctrl+0` 快捷键
  6. 字体设置持久化到 `RecentRecordManager` 或 `QSettings`
- **新增成员**：`QFontComboBox *m_fontCombo`, `QSpinBox *m_fontSizeSpin`
- **Qt API**：`QFontComboBox`, `QFont`, `QSpinBox`, `QSettings`
- **风险**：低 — 简单属性设置

---

## 依赖关系图

```
Phase 1 (核心修复)
├── 1.1 行号 ─────────────────────────────────────────────┐
├── 1.2 Minimap 绘制 ─────────────────────────────────── │
├── 1.3 大文件渲染 ──────────────────────────────────────│──→ Phase 3
└── 1.4 滚动同步 ─────────────────────────────────────── │
                                                          │
Phase 2 (算法增强)                                        │
├── 2.1 maxExactLines ────── 独立                        │
├── 2.2 coalesce 配对 ───── 独立                         │
├── 2.3 编码检测 ──────────→ 影响文件加载 → Phase 3      │
└── 2.4 二进制高亮 ──────── 依赖 2.2 配对结果            │
                                                          │
Phase 3 (交互提升) ← 依赖 Phase 1 渲染稳定性 ──────────────┘
├── 3.1 转到行 ─────────── 依赖 1.1 行号                 │
├── 3.2 交换保留位置 ───── 独立                          │
├── 3.3 预览提示 ───────── 独立                          │
├── 3.4 拖拽 ───────────── 依赖 2.3 编码检测             │
└── 3.5 快捷键 ─────────── 依赖 3.1 转到行               │
                                                          │
Phase 4 (功能扩展) ← 依赖 Phase 3 交互基础                │
├── 4.1 合并编辑 ───────── 依赖 1.4 / 3.2                │
├── 4.2 折叠展开 ───────── 依赖 1.3 渲染机制              │
├── 4.3 图标 ───────────── 独立                          │
├── 4.4 状态卡片 ───────── 独立                          │
└── 4.5 导出 ───────────── 依赖 Phase 2 diff 结果        │
                                                          │
Phase 5 (架构优化) ← 可与其他阶段并行                     │
├── 5.1 DiffView ───────── 独立（实验性）                │
├── 5.2 最近记录 ───────── 独立                          │
└── 5.3 字体调整 ───────── 独立                          │
```

---

## 测试策略

### 单元测试（`tests/`）
| 测试文件 | 新增测试用例 |
|----------|-------------|
| `file_compare_engine_tests.cpp` | `maxExactLines` 生效测试、`coalesceChangedRuns` 相似度配对测试、`UnifiedDiffExport` 格式测试、编码检测 BOM/GBK 测试 |
| `file_compare_page_tests.cpp` | 交换后 diff index 保持测试、拖拽事件测试、预览模式状态测试 |
| **新建** `diff_minimap_tests.cpp` | Minimap 缓存绘制验证、run-length merge 测试 |
| **新建** `diff_view_tests.cpp` | DiffView 虚拟滚动测试、行号渲染测试 |

### 集成测试
- 加载 10 万行文本文件，验证 F3/F7/Shift+F3 导航流畅
- 编码检测：GBK 中文文件、UTF-16LE 文件、混合编码
- 拖拽：同时拖入 2 个文件，验证左右分配正确
- 导出：patch 格式能被 `git apply --check` 接受

### 手工验证清单
- [ ] 行号在大文件（10000+ 行）时宽度正确（4→5位数字不截断）
- [ ] 滚动同步在两侧行数不同时按比例滚动
- [ ] 交换文件后差异导航位置不变
- [ ] 快捷键在编辑器获得焦点时仍能触发（需设置 `Qt::WidgetWithChildrenShortcut` 或 `Qt::WindowShortcut`）
- [ ] 深色/浅色主题下颜色正确

---

## 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| DiffView (5.1) 替换核心渲染，可能引入新bug | 高 | 通过编译宏 `EST_USE_DIFFVIEW` 作为可选特性，默认关闭；在设置中提供切换 |
| 大文件分块渲染 (1.3) 可能导致 ExtraSelections 闪烁 | 中 | 设置 `setUpdatesEnabled(false)` / `true` 包裹渲染过程 |
| 编码检测 (2.3) 误判导致乱码 | 中 | 提供手动编码覆盖下拉框；以 BOM 为最优先判断 |
| 拖拽支持 (3.4) 与 CodeEditor 内部拖放冲突 | 低 | 事件过滤器先于 CodeEditor 处理 |
| 键盘快捷键 (3.5) 与其他页面或系统快捷键冲突 | 低 | 使用 `Qt::WidgetWithChildrenShortcut` 限制作用域 |

---

## 附加说明

1. **代码风格**：所有新增代码应保持与现有代码一致的命名规范（驼峰命名、`m_` 成员前缀、匿名命名空间常量）。
2. **国际化**：所有用户可见字符串使用 `tr()` 包裹。
3. **CMake 更新**：Phase 4 新增 SVG 图标需更新 `resources.qrc`；Phase 5 新增 `DiffView` 需更新 `CMakeLists.txt`。
4. **渐进式交付**：每个阶段结束后可独立交付和验证，不阻塞其他功能开发。
5. **最近记录**（Phase 5.2）可与 Phase 2.3 的编码选择联动记录用户编码偏好。
