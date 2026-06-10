#include "FileComparePage.h"
#include "FileCompareEngine.h"
#include "widgets/IndustrialStatusCard.h"
#include "widgets/DiffMinimapWidget.h"
#include "widgets/CodeEditor.h"
#include "services/AppPaths.h"
#include "services/RecentRecordManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QMessageBox>
#include <QStackedWidget>
#include <QTextStream>
#include <QStringDecoder>
#include <QScrollBar>
#include <QTextBlock>
#include <algorithm>
#include <utility>
#include <QLineEdit>
#include <QIntValidator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QShortcut>
#include <QKeySequence>
#include <QFontComboBox>
#include <QSpinBox>
#include <QMenu>
#include <QAction>
#include <QDateTime>

namespace est
{

    namespace
    {
        enum CompareMode
        {
            AutoMode = 0,
            TextMode = 1,
            BinaryMode = 2
        };

        QString formatBytes(qint64 bytes)
        {
            if (bytes < 1024)
            {
                return QStringLiteral("%1 B").arg(bytes);
            }
            if (bytes < 1024 * 1024)
            {
                return QStringLiteral("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
            }
            return QStringLiteral("%1 MB").arg(QString::number(bytes / 1024.0 / 1024.0, 'f', 1));
        }

        QString fileKindLabel(bool binary)
        {
            return binary ? QObject::tr("二进制") : QObject::tr("文本");
        }

        QString fileCaption(const FileComparePage::LoadedFile &file, const QString &emptyText)
        {
            if (!file.loaded)
            {
                return emptyText;
            }

            return QStringLiteral("%1  |  %2  |  %3  |  %4")
                .arg(QFileInfo(file.path).fileName(),
                     formatBytes(file.size),
                     fileKindLabel(file.binary),
                     file.encoding.isEmpty() ? QStringLiteral("?") : file.encoding);
        }

        QColor colorForStatus(DiffLine::Status status, bool leftSide)
        {
            switch (status)
            {
            case DiffLine::Added:
                return leftSide ? QColor() : QColor(QStringLiteral("#DDF7E8"));
            case DiffLine::Removed:
                return leftSide ? QColor(QStringLiteral("#FCE1E1")) : QColor();
            case DiffLine::Modified:
                return QColor(QStringLiteral("#FFF3B0"));
            case DiffLine::Equal:
                return QColor();
            }
            return QColor();
        }

        QColor collapsedBlockColor()
        {
            return QColor(QStringLiteral("#EEF4F9"));
        }

        QColor modifiedInlineColor()
        {
            return QColor(QStringLiteral("#F8D879"));
        }

        QString collapsedBlockText(int hiddenLines)
        {
            return QObject::tr("--- 已折叠 %1 行相同内容 ---").arg(hiddenLines);
        }

        QPair<int, int> changedRange(const QString &current, const QString &peer)
        {
            const int currentLength = static_cast<int>(current.size());
            const int peerLength = static_cast<int>(peer.size());
            const int commonLength = std::min(currentLength, peerLength);
            int prefix = 0;
            while (prefix < commonLength && current.at(prefix) == peer.at(prefix))
            {
                ++prefix;
            }

            int suffix = 0;
            while (suffix < commonLength - prefix && current.at(currentLength - 1 - suffix) == peer.at(peerLength - 1 - suffix))
            {
                ++suffix;
            }

            const int length = std::max(1, currentLength - prefix - suffix);
            return {prefix, length};
        }
    }

    FileComparePage::FileComparePage(QWidget *parent)
        : QWidget(parent)
    {
        m_renderTimer = new QTimer(this);
        m_renderTimer->setSingleShot(true);
        connect(m_renderTimer, &QTimer::timeout, this, &FileComparePage::renderNextChunk);
        setupUi();
    }

    void FileComparePage::setupUi()
    {
        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(14, 12, 14, 14);
        mainLayout->setSpacing(12);

        auto *toolbarFrame = new QFrame(this);
        toolbarFrame->setObjectName(QStringLiteral("compareToolbar"));
        toolbarFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto *toolbarLayout = new QVBoxLayout(toolbarFrame);
        toolbarLayout->setContentsMargins(0, 0, 0, 0);
        toolbarLayout->setSpacing(8);

        auto *actionLayout = new QHBoxLayout();
        actionLayout->setSpacing(8);
        m_btnOpenLeft = new QPushButton(tr("📂 打开左侧"), this);
        m_btnOpenRight = new QPushButton(tr("📂 打开右侧"), this);
        m_btnCompare = new QPushButton(tr("⇔ 比较"), this);
        m_btnSwap = new QPushButton(tr("⇄ 交换"), this);
        m_btnClear = new QPushButton(tr("✕ 清空"), this);
        m_btnPrevDiff = new QPushButton(tr("◀ 上一处"), this);
        m_btnNextDiff = new QPushButton(tr("下一处 ▶"), this);

        m_btnCompare->setObjectName(QStringLiteral("primaryActionButton"));
        m_btnPrevDiff->setObjectName(QStringLiteral("comparePrevDiffButton"));
        m_btnNextDiff->setObjectName(QStringLiteral("compareNextDiffButton"));

        // P4.1: 接受左右侧差异块
        m_btnAcceptLeft = new QPushButton(tr("← 接受左侧"), this);
        m_btnAcceptRight = new QPushButton(tr("接受右侧 →"), this);

        // P4.2: 展开全部折叠行
        auto *btnExpandAll = new QPushButton(tr("↕ 展开全部"), this);

        // P4.5: 导出按钮（带菜单）
        m_btnExport = new QPushButton(tr("📋 导出"), this);

        // P5.2: 最近比较记录
        m_btnRecent = new QPushButton(tr("🕐 最近"), this);

        m_modeCombo = new QComboBox(this);
        m_modeCombo->setObjectName(QStringLiteral("compareModeCombo"));
        m_modeCombo->addItem(tr("自动识别"), AutoMode);
        m_modeCombo->addItem(tr("文本"), TextMode);
        m_modeCombo->addItem(tr("二进制"), BinaryMode);

        m_ignoreWhitespaceCheckBox = new QCheckBox(tr("忽略空白"), this);
        m_ignoreCaseCheckBox = new QCheckBox(tr("忽略大小写"), this);
        m_showOnlyDiffsCheckBox = new QCheckBox(tr("只看差异"), this);

        actionLayout->addWidget(m_btnOpenLeft);
        actionLayout->addWidget(m_btnOpenRight);
        actionLayout->addWidget(m_btnCompare);
        actionLayout->addWidget(m_btnSwap);
        actionLayout->addWidget(m_btnClear);
        actionLayout->addSpacing(12);
        actionLayout->addWidget(m_btnPrevDiff);
        actionLayout->addWidget(m_btnNextDiff);
        actionLayout->addWidget(m_btnAcceptLeft);
        actionLayout->addWidget(m_btnAcceptRight);
        actionLayout->addWidget(btnExpandAll);
        actionLayout->addWidget(m_btnExport);
        actionLayout->addWidget(m_btnRecent);
        actionLayout->addStretch(1);

        auto *optionLayout = new QHBoxLayout();
        optionLayout->setSpacing(12);
        auto *modeLabel = new QLabel(tr("比较模式"), this);
        modeLabel->setObjectName(QStringLiteral("compareOptionLabel"));
        optionLayout->addWidget(modeLabel);
        optionLayout->addWidget(m_modeCombo);
        auto *encodingLabel = new QLabel(tr("编码"), this);
        encodingLabel->setObjectName(QStringLiteral("compareOptionLabel"));
        m_encodingCombo = new QComboBox(this);
        m_encodingCombo->addItem(tr("自动检测"));
        m_encodingCombo->addItem(QStringLiteral("UTF-8"));
        m_encodingCombo->addItem(QStringLiteral("GBK"));
        m_encodingCombo->addItem(QStringLiteral("UTF-16LE"));
        m_encodingCombo->addItem(QStringLiteral("UTF-16BE"));
        m_encodingCombo->addItem(QStringLiteral("System"));
        optionLayout->addWidget(encodingLabel);
        optionLayout->addWidget(m_encodingCombo);
        auto *fontLabel = new QLabel(tr("字体"), this);
        fontLabel->setObjectName(QStringLiteral("compareOptionLabel"));
        m_fontCombo = new QFontComboBox(this);
        m_fontCombo->setMaximumWidth(160);
        m_fontCombo->setCurrentFont(QFont(QStringLiteral("JetBrains Mono"), 10));
        m_fontSizeSpin = new QSpinBox(this);
        m_fontSizeSpin->setRange(6, 48);
        m_fontSizeSpin->setValue(10);
        m_fontSizeSpin->setMaximumWidth(50);
        m_fontSizeSpin->setSuffix(QStringLiteral("pt"));
        optionLayout->addWidget(fontLabel);
        optionLayout->addWidget(m_fontCombo);
        optionLayout->addWidget(m_fontSizeSpin);
        optionLayout->addWidget(m_ignoreWhitespaceCheckBox);
        optionLayout->addWidget(m_ignoreCaseCheckBox);
        optionLayout->addWidget(m_showOnlyDiffsCheckBox);

        auto *gotoLabel = new QLabel(tr("转到行:"), this);
        gotoLabel->setObjectName(QStringLiteral("compareOptionLabel"));
        m_gotoLineEdit = new QLineEdit(this);
        m_gotoLineEdit->setPlaceholderText(tr("行号"));
        m_gotoLineEdit->setMaximumWidth(80);
        m_gotoLineEdit->setValidator(new QIntValidator(1, 999999, this));
        auto *gotoBtn = new QPushButton(tr("跳转"), this);
        gotoBtn->setObjectName(QStringLiteral("compareGoToButton"));
        optionLayout->addWidget(gotoLabel);
        optionLayout->addWidget(m_gotoLineEdit);
        optionLayout->addWidget(gotoBtn);

        optionLayout->addStretch(1);

        toolbarLayout->addLayout(actionLayout);
        toolbarLayout->addLayout(optionLayout);
        mainLayout->addWidget(toolbarFrame);

        auto *statusLayout = new QHBoxLayout();
        statusLayout->setContentsMargins(0, 0, 0, 0);
        statusLayout->setSpacing(10);
        m_leftStatusCard = new IndustrialStatusCard(tr("左侧文件"), this);
        m_compareStatusCard = new IndustrialStatusCard(tr("比较结果"), this);
        m_rightStatusCard = new IndustrialStatusCard(tr("右侧文件"), this);
        m_compareStatusCard->setMinimumWidth(300);
        statusLayout->addWidget(m_leftStatusCard, 1);
        statusLayout->addWidget(m_compareStatusCard, 1);
        statusLayout->addWidget(m_rightStatusCard, 1);
        mainLayout->addLayout(statusLayout);

        m_contentStack = new QStackedWidget(this);
        m_contentStack->setObjectName(QStringLiteral("compareContentStack"));

        auto *emptyState = new QFrame(m_contentStack);
        emptyState->setObjectName(QStringLiteral("compareEmptyState"));
        auto *emptyLayout = new QVBoxLayout(emptyState);
        emptyLayout->setContentsMargins(36, 32, 36, 32);
        emptyLayout->setSpacing(12);
        emptyLayout->addStretch(1);

        auto *emptyTitle = new QLabel(tr("选择两个文件开始比较"), emptyState);
        emptyTitle->setObjectName(QStringLiteral("sectionTitle"));
        emptyTitle->setAlignment(Qt::AlignCenter);

        auto *emptyDetail = new QLabel(
            tr("支持文本文件逐行比较，也支持二进制文件的十六进制视图。加载单侧文件时会先预览内容，双侧都就绪后再按变更块比较。"),
            emptyState);
        emptyDetail->setObjectName(QStringLiteral("statusCardDetail"));
        emptyDetail->setWordWrap(true);
        emptyDetail->setAlignment(Qt::AlignCenter);

        auto *emptyHint = new QLabel(
            tr("建议优先使用“只看差异”聚焦变更块；该模式会保留上下文并折叠相同内容，避免阅读时丢失定位。"),
            emptyState);
        emptyHint->setObjectName(QStringLiteral("statusCardDetail"));
        emptyHint->setWordWrap(true);
        emptyHint->setAlignment(Qt::AlignCenter);

        emptyLayout->addWidget(emptyTitle, 0, Qt::AlignHCenter);
        emptyLayout->addWidget(emptyDetail, 0, Qt::AlignHCenter);
        emptyLayout->addWidget(emptyHint, 0, Qt::AlignHCenter);
        emptyLayout->addStretch(1);

        auto *compareWorkspace = new QFrame(m_contentStack);
        compareWorkspace->setObjectName(QStringLiteral("compareWorkspace"));
        auto *compareLayout = new QVBoxLayout(compareWorkspace);
        compareLayout->setContentsMargins(0, 0, 0, 0);
        compareLayout->setSpacing(10);

        auto *summaryFrame = new QFrame(compareWorkspace);
        summaryFrame->setObjectName(QStringLiteral("compareSummaryFrame"));
        auto *summaryLayout = new QHBoxLayout(summaryFrame);
        summaryLayout->setContentsMargins(12, 10, 12, 10);
        summaryLayout->setSpacing(8);
        m_summaryLabel = new QLabel(tr("请选择两个文件进行比较"), summaryFrame);
        m_summaryLabel->setObjectName(QStringLiteral("compareSummaryLabel"));
        m_summaryLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        m_summaryLabel->setWordWrap(true);

        auto makeLegendChip = [summaryFrame](const QString &text, const QString &objectName)
        {
            auto *chip = new QLabel(text, summaryFrame);
            chip->setObjectName(objectName);
            chip->setAlignment(Qt::AlignCenter);
            return chip;
        };

        summaryLayout->addWidget(m_summaryLabel, 1);
        summaryLayout->addWidget(makeLegendChip(tr("新增"), QStringLiteral("compareLegendAdded")));
        summaryLayout->addWidget(makeLegendChip(tr("删除"), QStringLiteral("compareLegendRemoved")));
        summaryLayout->addWidget(makeLegendChip(tr("修改"), QStringLiteral("compareLegendModified")));
        compareLayout->addWidget(summaryFrame);

        m_splitter = new QSplitter(Qt::Horizontal, compareWorkspace);
        m_splitter->setObjectName(QStringLiteral("compareSplitter"));
        m_splitter->setChildrenCollapsible(false);
        m_splitter->setHandleWidth(10);

        auto *leftPanel = new QFrame(m_splitter);
        leftPanel->setObjectName(QStringLiteral("compareFilePanel"));
        auto *leftPanelLayout = new QVBoxLayout(leftPanel);
        leftPanelLayout->setContentsMargins(0, 0, 0, 0);
        leftPanelLayout->setSpacing(8);
        auto *leftHeader = new QFrame(leftPanel);
        leftHeader->setObjectName(QStringLiteral("compareFileHeader"));
        auto *leftHeaderLayout = new QVBoxLayout(leftHeader);
        leftHeaderLayout->setContentsMargins(12, 10, 12, 10);
        leftHeaderLayout->setSpacing(4);
        m_leftFileLabel = new QLabel(tr("左侧：未选择文件"), leftHeader);
        m_leftFileLabel->setObjectName(QStringLiteral("compareLeftFileLabel"));
        m_leftFileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_leftPathLabel = new QLabel(tr("路径：--"), leftHeader);
        m_leftPathLabel->setObjectName(QStringLiteral("compareFilePathLabel"));
        m_leftPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_leftPathLabel->setWordWrap(true);
        leftHeaderLayout->addWidget(m_leftFileLabel);
        leftHeaderLayout->addWidget(m_leftPathLabel);
        m_leftEditor = new CodeEditor(leftPanel);
        m_leftEditor->setObjectName(QStringLiteral("compareLeftEditor"));
        m_leftEditor->setReadOnly(true);
        m_leftEditor->setMinimumHeight(320);
        leftPanelLayout->addWidget(leftHeader);
        leftPanelLayout->addWidget(m_leftEditor, 1);

        m_minimap = new DiffMinimapWidget(m_splitter);

        auto *rightPanel = new QFrame(m_splitter);
        rightPanel->setObjectName(QStringLiteral("compareFilePanel"));
        auto *rightPanelLayout = new QVBoxLayout(rightPanel);
        rightPanelLayout->setContentsMargins(0, 0, 0, 0);
        rightPanelLayout->setSpacing(8);
        auto *rightHeader = new QFrame(rightPanel);
        rightHeader->setObjectName(QStringLiteral("compareFileHeader"));
        auto *rightHeaderLayout = new QVBoxLayout(rightHeader);
        rightHeaderLayout->setContentsMargins(12, 10, 12, 10);
        rightHeaderLayout->setSpacing(4);
        m_rightFileLabel = new QLabel(tr("右侧：未选择文件"), rightHeader);
        m_rightFileLabel->setObjectName(QStringLiteral("compareRightFileLabel"));
        m_rightFileLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_rightPathLabel = new QLabel(tr("路径：--"), rightHeader);
        m_rightPathLabel->setObjectName(QStringLiteral("compareFilePathLabel"));
        m_rightPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_rightPathLabel->setWordWrap(true);
        rightHeaderLayout->addWidget(m_rightFileLabel);
        rightHeaderLayout->addWidget(m_rightPathLabel);
        m_rightEditor = new CodeEditor(rightPanel);
        m_rightEditor->setObjectName(QStringLiteral("compareRightEditor"));
        m_rightEditor->setReadOnly(true);
        m_rightEditor->setMinimumHeight(320);
        rightPanelLayout->addWidget(rightHeader);
        rightPanelLayout->addWidget(m_rightEditor, 1);

        m_splitter->addWidget(leftPanel);
        m_splitter->addWidget(m_minimap);
        m_splitter->addWidget(rightPanel);
        m_splitter->setStretchFactor(0, 1);
        m_splitter->setStretchFactor(1, 0);
        m_splitter->setStretchFactor(2, 1);
        m_splitter->setSizes({600, 34, 600});
        compareLayout->addWidget(m_splitter, 1);

        m_contentStack->addWidget(emptyState);
        m_contentStack->addWidget(compareWorkspace);
        mainLayout->addWidget(m_contentStack, 1);

        // 垂直滚动同步（比例同步，避免递归）
        connect(m_leftEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
            syncScrollBar(m_leftEditor->verticalScrollBar(), m_rightEditor->verticalScrollBar());
        });
        connect(m_rightEditor->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
            syncScrollBar(m_rightEditor->verticalScrollBar(), m_leftEditor->verticalScrollBar());
        });

        // 水平滚动同步（比例同步，避免递归）
        connect(m_leftEditor->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
            syncScrollBar(m_leftEditor->horizontalScrollBar(), m_rightEditor->horizontalScrollBar());
        });
        connect(m_rightEditor->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this](int) {
            syncScrollBar(m_rightEditor->horizontalScrollBar(), m_leftEditor->horizontalScrollBar());
        });

        connect(m_minimap, &DiffMinimapWidget::lineClicked, this, [this](int lineIndex)
                {
        QTextBlock blockL = m_leftEditor->document()->findBlockByNumber(lineIndex);
        if (blockL.isValid()) {
            QTextCursor cursor(blockL);
            m_leftEditor->setTextCursor(cursor);
            m_leftEditor->centerCursor();
        }
        QTextBlock blockR = m_rightEditor->document()->findBlockByNumber(lineIndex);
        if (blockR.isValid()) {
            QTextCursor cursor(blockR);
            m_rightEditor->setTextCursor(cursor);
            m_rightEditor->centerCursor();
        } });

        connect(m_btnOpenLeft, &QPushButton::clicked, this, [this]()
                { openFile(true); });
        connect(m_btnOpenRight, &QPushButton::clicked, this, [this]()
                { openFile(false); });
        connect(m_btnCompare, &QPushButton::clicked, this, &FileComparePage::compareFiles);
        connect(m_btnSwap, &QPushButton::clicked, this, &FileComparePage::swapFiles);
        connect(m_btnClear, &QPushButton::clicked, this, &FileComparePage::clearComparison);
        connect(m_btnPrevDiff, &QPushButton::clicked, this, [this]()
                { navigateDiff(-1); });
        connect(m_btnNextDiff, &QPushButton::clicked, this, [this]()
                { navigateDiff(1); });

        // P4.1: 接受左右侧差异块
        connect(m_btnAcceptLeft, &QPushButton::clicked, this, [this]()
                { acceptCurrentBlock(true); });
        connect(m_btnAcceptRight, &QPushButton::clicked, this, [this]()
                { acceptCurrentBlock(false); });

        // P4.2: 展开全部折叠行
        connect(btnExpandAll, &QPushButton::clicked, this, [this]()
                {
            m_expandedBlocks.clear();
            m_collapsedBlockMap.clear();
            m_showOnlyDiffsCheckBox->setChecked(false);
            renderDiffs(); });

        // P4.5: 导出菜单
        {
            auto *exportMenu = new QMenu(this);
            auto *actionPatch = exportMenu->addAction(tr("导出 Unified Diff (.patch)"));
            auto *actionHtml = exportMenu->addAction(tr("导出 HTML 报告"));
            auto *actionSummary = exportMenu->addAction(tr("导出文本摘要"));
            m_btnExport->setMenu(exportMenu);

            connect(actionPatch, &QAction::triggered, this, [this]()
                    {
                if (!m_leftFile.loaded || !m_rightFile.loaded || m_allDiffs.isEmpty()) return;
                QString savePath = QFileDialog::getSaveFileName(this, tr("导出 Unified Diff"), AppPaths::exportFilePath(QStringLiteral("diff.patch")), tr("Patch 文件 (*.patch)"));
                if (savePath.isEmpty()) return;
                if (AppPaths::isDriveCPath(savePath)) {
                    QMessageBox::warning(this, tr("导出失败"), tr("保存路径不能在 C 盘，请选择软件目录下的数据文件夹"));
                    return;
                }
                QString patch = FileCompareEngine::exportUnifiedDiff(
                    m_leftFile.lines, m_rightFile.lines,
                    QFileInfo(m_leftFile.path).fileName(), QFileInfo(m_rightFile.path).fileName(),
                    m_allDiffs);
                QFile f(savePath);
                if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    f.write(patch.toUtf8());
                    f.close();
                } });

            connect(actionHtml, &QAction::triggered, this, [this]()
                    {
                if (m_allDiffs.isEmpty()) return;
                QString savePath = QFileDialog::getSaveFileName(this, tr("导出 HTML 报告"), AppPaths::exportFilePath(QStringLiteral("diff_report.html")), tr("HTML 文件 (*.html)"));
                if (savePath.isEmpty()) return;
                if (AppPaths::isDriveCPath(savePath)) {
                    QMessageBox::warning(this, tr("导出失败"), tr("保存路径不能在 C 盘，请选择软件目录下的数据文件夹"));
                    return;
                }
                QString html = FileCompareEngine::exportHtmlDiff(m_allDiffs,
                    m_leftFile.loaded ? QFileInfo(m_leftFile.path).fileName() : tr("无"),
                    m_rightFile.loaded ? QFileInfo(m_rightFile.path).fileName() : tr("无"));
                QFile f(savePath);
                if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    f.write(html.toUtf8());
                    f.close();
                } });

            connect(actionSummary, &QAction::triggered, this, [this]()
                    {
                if (m_allDiffs.isEmpty()) return;
                QString savePath = QFileDialog::getSaveFileName(this, tr("导出文本摘要"), AppPaths::exportFilePath(QStringLiteral("diff_summary.txt")), tr("文本文件 (*.txt)"));
                if (savePath.isEmpty()) return;
                if (AppPaths::isDriveCPath(savePath)) {
                    QMessageBox::warning(this, tr("导出失败"), tr("保存路径不能在 C 盘，请选择软件目录下的数据文件夹"));
                    return;
                }
                QString summary = FileCompareEngine::exportTextSummary(m_allDiffs);
                QFile f(savePath);
                if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    f.write(summary.toUtf8());
                    f.close();
                } });
        }

        // P5.2: 最近比较记录按钮
        connect(m_btnRecent, &QPushButton::clicked, this, &FileComparePage::refreshRecentMenu);

        // P5.3: 字体控制信号
        auto applyFont = [this]()
        {
            QFont f = m_fontCombo->currentFont();
            f.setPointSize(m_fontSizeSpin->value());
            m_leftEditor->setEditorFont(f);
            m_rightEditor->setEditorFont(f);
        };
        connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, applyFont);
        connect(m_fontSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [applyFont](int)
                { applyFont(); });

        // P4.2: 光标移至折叠行时自动展开
        auto makeFoldExpander = [this](CodeEditor *editor)
        {
            connect(editor, &QPlainTextEdit::cursorPositionChanged, this, [this, editor]()
                    {
                if (m_expandingFold) return;
                QTextCursor cursor = editor->textCursor();
                int blockNum = cursor.blockNumber();
                auto it = m_collapsedBlockMap.find(blockNum);
                if (it != m_collapsedBlockMap.end()) {
                    m_expandingFold = true;
                    m_expandedBlocks.insert(it.value());
                    m_collapsedBlockMap.remove(blockNum);
                    renderDiffs();
                    // m_expandingFold 由 renderNextChunk 完成后清除
                } });
        };
        makeFoldExpander(m_leftEditor);
        makeFoldExpander(m_rightEditor);
        connect(m_ignoreWhitespaceCheckBox, &QCheckBox::toggled, this, &FileComparePage::compareFiles);
        connect(m_ignoreCaseCheckBox, &QCheckBox::toggled, this, &FileComparePage::compareFiles);
        connect(m_showOnlyDiffsCheckBox, &QCheckBox::toggled, this, &FileComparePage::renderDiffs);
        connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this]()
                {
        if (m_leftFile.loaded && !m_leftFile.path.isEmpty()) {
            QString error;
            m_leftFile = loadFileContent(m_leftFile.path, &error);
        }
        if (m_rightFile.loaded && !m_rightFile.path.isEmpty()) {
            QString error;
            m_rightFile = loadFileContent(m_rightFile.path, &error);
        }
        updateFileLabels();
        compareFiles(); });

        connect(m_encodingCombo, &QComboBox::currentIndexChanged, this, [this]()
                {
        if (m_leftFile.loaded && !m_leftFile.path.isEmpty() && !m_leftFile.binary) {
            QString error;
            m_leftFile = loadFileContent(m_leftFile.path, &error);
        }
        if (m_rightFile.loaded && !m_rightFile.path.isEmpty() && !m_rightFile.binary) {
            QString error;
            m_rightFile = loadFileContent(m_rightFile.path, &error);
        }
        updateFileLabels();
        compareFiles(); });

        // 转到行信号连接
        auto gotoLine = [this]() {
            if (!m_gotoLineEdit) return;
            bool ok = false;
            int lineNo = m_gotoLineEdit->text().toInt(&ok);
            if (!ok || lineNo < 1) return;
            int lineIdx = lineNo - 1;
            QTextBlock blockL = m_leftEditor->document()->findBlockByNumber(lineIdx);
            if (blockL.isValid()) {
                m_leftEditor->setTextCursor(QTextCursor(blockL));
                m_leftEditor->centerCursor();
            }
            QTextBlock blockR = m_rightEditor->document()->findBlockByNumber(lineIdx);
            if (blockR.isValid()) {
                m_rightEditor->setTextCursor(QTextCursor(blockR));
                m_rightEditor->centerCursor();
            }
            m_gotoLineEdit->selectAll();
        };
        // gotoBtn is created above in optionLayout section, find and connect it
        if (auto *btn = findChild<QPushButton *>(QStringLiteral("compareGoToButton"))) {
            connect(btn, &QPushButton::clicked, this, gotoLine);
        }
        connect(m_gotoLineEdit, &QLineEdit::returnPressed, this, gotoLine);

        // 文件拖拽支持
        setAcceptDrops(true);
        m_leftEditor->viewport()->setAcceptDrops(true);
        m_rightEditor->viewport()->setAcceptDrops(true);

        // 键盘快捷键
        auto *shortcutOpenLeft = new QShortcut(QKeySequence(QStringLiteral("Ctrl+O")), this);
        connect(shortcutOpenLeft, &QShortcut::activated, this, [this]() { openFile(true); });

        auto *shortcutOpenRight = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")), this);
        connect(shortcutOpenRight, &QShortcut::activated, this, [this]() { openFile(false); });

        auto *shortcutRecompare = new QShortcut(QKeySequence(QStringLiteral("F5")), this);
        connect(shortcutRecompare, &QShortcut::activated, this, &FileComparePage::compareFiles);

        auto *shortcutNextDiff = new QShortcut(QKeySequence(QStringLiteral("F3")), this);
        connect(shortcutNextDiff, &QShortcut::activated, this, [this]() { navigateDiff(1); });

        auto *shortcutPrevDiff = new QShortcut(QKeySequence(QStringLiteral("Shift+F3")), this);
        connect(shortcutPrevDiff, &QShortcut::activated, this, [this]() { navigateDiff(-1); });

        auto *shortcutGoto = new QShortcut(QKeySequence(QStringLiteral("Ctrl+G")), this);
        connect(shortcutGoto, &QShortcut::activated, this, [this]() {
            if (m_gotoLineEdit) {
                m_gotoLineEdit->setFocus();
                m_gotoLineEdit->selectAll();
            }
        });

        auto *shortcutToggleDiffs = new QShortcut(QKeySequence(QStringLiteral("Ctrl+D")), this);
        connect(shortcutToggleDiffs, &QShortcut::activated, this, [this]() {
            m_showOnlyDiffsCheckBox->toggle();
        });

        auto *shortcutSwap = new QShortcut(QKeySequence(QStringLiteral("Ctrl+S")), this);
        connect(shortcutSwap, &QShortcut::activated, this, &FileComparePage::swapFiles);

        auto *shortcutClear = new QShortcut(QKeySequence(QStringLiteral("Escape")), this);
        connect(shortcutClear, &QShortcut::activated, this, &FileComparePage::clearComparison);

        updateFileLabels();
        updateVisibleState();
        updateStatusCards();
        updateActionStates();
    }

    void FileComparePage::openFile(bool leftSide)
    {
        const QString path = QFileDialog::getOpenFileName(this, leftSide ? tr("打开左侧文件") : tr("打开右侧文件"));
        if (path.isEmpty())
        {
            return;
        }

        loadFileFromPath(path, leftSide);
    }

    void FileComparePage::loadFileFromPath(const QString &path, bool leftSide)
    {
        QString errorMessage;
        LoadedFile loadedFile = loadFileContent(path, &errorMessage);
        if (!errorMessage.isEmpty()) {
            QMessageBox::warning(this, tr("文件读取失败"), errorMessage);
            return;
        }
        if (leftSide) {
            m_leftFile = loadedFile;
        } else {
            m_rightFile = loadedFile;
        }
        updateFileLabels();
        compareFiles();
    }

    FileComparePage::LoadedFile FileComparePage::loadFileContent(const QString &path, QString *errorMessage) const
    {
        LoadedFile loadedFile;
        loadedFile.path = path;

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = tr("无法打开文件：%1").arg(path);
            }
            return loadedFile;
        }

        loadedFile.size = file.size();
        const QString ext = QFileInfo(path).suffix().toLower();
        const QStringList binExts = {"bin", "exe", "dll", "so", "img", "o", "obj"};
        const QStringList textExts = {"txt", "c", "cpp", "h", "hpp", "json", "xml", "csv", "md", "log", "ini", "hex", "srec"};
        const int selectedMode = m_modeCombo != nullptr ? m_modeCombo->currentData().toInt() : AutoMode;

        bool isBinary = false;
        if (selectedMode == BinaryMode)
        {
            isBinary = true;
        }
        else if (selectedMode == TextMode)
        {
            isBinary = false;
        }
        else if (binExts.contains(ext))
        {
            isBinary = true;
        }
        else if (textExts.contains(ext))
        {
            isBinary = false;
        }
        else
        {
            QByteArray firstBytes = file.read(4096);
            isBinary = firstBytes.contains('\0');
            file.seek(0);
        }

        loadedFile.binary = isBinary;
        if (isBinary)
        {
            qint64 offset = 0;
            while (!file.atEnd())
            {
                QByteArray chunk = file.read(16);
                if (chunk.isEmpty())
                    break;

                QString line = QString("%1: ").arg(offset, 8, 16, QChar('0')).toUpper();
                QString hexPart;
                QString asciiPart;
                for (int i = 0; i < chunk.size(); ++i)
                {
                    unsigned char c = static_cast<unsigned char>(chunk[i]);
                    hexPart += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();
                    if (c >= 32 && c <= 126)
                        asciiPart += QChar(c);
                    else
                        asciiPart += '.';
                }
                hexPart = hexPart.leftJustified(48, ' ');
                line += hexPart + "| " + asciiPart;
                loadedFile.lines.append(line);
                offset += chunk.size();
            }
        }
        else
        {
            QByteArray rawData = file.readAll();
            loadedFile.encoding = encoding::detectEncoding(rawData);

            // 如果用户手动选择了编码，优先使用
            if (m_encodingCombo && m_encodingCombo->currentIndex() > 0) {
                loadedFile.encoding = m_encodingCombo->currentText();
            }

            // 使用 QStringDecoder 解码
            QStringDecoder decoder;
            if (loadedFile.encoding == QStringLiteral("System")) {
                decoder = QStringDecoder(QStringConverter::System);
            } else {
                auto optEnc = QStringConverter::encodingForName(loadedFile.encoding.toUtf8().constData());
                decoder = optEnc ? QStringDecoder(*optEnc) : QStringDecoder(QStringConverter::Utf8);
            }
            if (!decoder.isValid()) {
                decoder = QStringDecoder(QStringConverter::Utf8); // 兜底 UTF-8
            }
            QString text = decoder.decode(rawData);
            loadedFile.lines = text.split('\n');
        }

        loadedFile.loaded = true;
        return loadedFile;
    }

    void FileComparePage::compareFiles()
    {
        if (!m_leftFile.loaded && !m_rightFile.loaded)
        {
            clearComparison();
            return;
        }

        if (!m_leftFile.loaded || !m_rightFile.loaded)
        {
            m_allDiffs.clear();
            m_currentDiffIndex = -1;
            renderDiffs();
            return;
        }

        FileCompareOptions options;
        options.ignoreWhitespace = m_ignoreWhitespaceCheckBox->isChecked();
        options.ignoreCase = m_ignoreCaseCheckBox->isChecked();

        m_allDiffs = FileCompareEngine::compare(m_leftFile.lines, m_rightFile.lines, options);
        m_currentDiffIndex = -1;

        // P5.2: 记录最近比较对
        if (m_recentRecordManager && m_leftFile.loaded && m_rightFile.loaded) {
            m_recentRecordManager->addComparePair(m_leftFile.path, m_rightFile.path,
                                                   m_leftFile.size, m_rightFile.size);
            refreshRecentMenu();
        }

        renderDiffs();
    }

    void FileComparePage::renderDiffs()
    {
        m_expandingFold = true;  // P4.2: 防止渲染过程中误触发展开
        m_leftEditor->clear();
        m_rightEditor->clear();
        m_renderedDiffs.clear();
        m_diffBlockIndexes.clear();
        m_collapsedBlockMap.clear();
        m_addedCount = 0;
        m_removedCount = 0;
        m_modifiedCount = 0;
        m_equalCount = 0;
        m_diffBlockCount = 0;

        QList<RenderLine> renderLines;
        QStringList leftLines;
        QStringList rightLines;

        const bool hasBothFiles = m_leftFile.loaded && m_rightFile.loaded;
        if (!hasBothFiles)
        {
            const bool leftLoaded = m_leftFile.loaded;
            const QStringList &previewLines = leftLoaded ? m_leftFile.lines : m_rightFile.lines;
            renderLines.reserve(previewLines.size());
            for (const QString &line : previewLines)
            {
                renderLines.append({{leftLoaded ? line : QString(), leftLoaded ? QString() : line, DiffLine::Equal}, QColor(), QColor(), false});
                leftLines.append(leftLoaded ? line : QString());
                rightLines.append(leftLoaded ? QString() : line);
                ++m_equalCount;
            }
        }
        else
        {
            const QList<QPair<int, int>> diffBlocks = collectDiffBlocks(m_allDiffs);
            m_diffBlockCount = diffBlocks.size();

            for (const DiffLine &diff : m_allDiffs)
            {
                switch (diff.status)
                {
                case DiffLine::Added:
                    ++m_addedCount;
                    break;
                case DiffLine::Removed:
                    ++m_removedCount;
                    break;
                case DiffLine::Modified:
                    ++m_modifiedCount;
                    break;
                case DiffLine::Equal:
                    ++m_equalCount;
                    break;
                }
            }

            QList<QPair<int, int>> visibleRanges;
            const bool showOnlyDiffs = m_showOnlyDiffsCheckBox->isChecked() && !diffBlocks.isEmpty();
            if (showOnlyDiffs)
            {
                constexpr int contextLines = 2;
                for (const auto &block : diffBlocks)
                {
                    const int start = std::max(0, block.first - contextLines);
                    const int end = std::min(static_cast<int>(m_allDiffs.size()), block.second + contextLines);
                    if (!visibleRanges.isEmpty() && start <= visibleRanges.last().second)
                    {
                        visibleRanges.last().second = std::max(visibleRanges.last().second, end);
                    }
                    else
                    {
                        visibleRanges.append({start, end});
                    }
                }
            }
            else if (!m_allDiffs.isEmpty())
            {
                visibleRanges.append({0, m_allDiffs.size()});
            }

            int nextBlockIndex = 0;
            for (int rangeIndex = 0; rangeIndex < visibleRanges.size(); ++rangeIndex)
            {
                const auto &range = visibleRanges.at(rangeIndex);
                if (rangeIndex > 0)
                {
                    const int prevEnd = visibleRanges.at(rangeIndex - 1).second;
                    const int hiddenLines = range.first - prevEnd;
                    if (m_expandedBlocks.contains(rangeIndex))
                    {
                        // P4.2: 该折叠块已被用户展开，直接渲染所有行
                        for (int sourceIndex = prevEnd; sourceIndex < range.first; ++sourceIndex)
                        {
                            const DiffLine &diff = m_allDiffs.at(sourceIndex);
                            renderLines.append({diff, QColor(), QColor(), false});
                            leftLines.append(diff.textA);
                            rightLines.append(diff.textB);
                            m_renderedDiffs.append(diff);
                        }
                    }
                    else
                    {
                        const DiffLine placeholder{collapsedBlockText(hiddenLines), collapsedBlockText(hiddenLines), DiffLine::Equal};
                        m_collapsedBlockMap[renderLines.size()] = rangeIndex;
                        renderLines.append({placeholder, collapsedBlockColor(), collapsedBlockColor(), true});
                        leftLines.append(placeholder.textA);
                        rightLines.append(placeholder.textB);
                        m_renderedDiffs.append(placeholder);
                    }
                }

                for (int sourceIndex = range.first; sourceIndex < range.second; ++sourceIndex)
                {
                    while (nextBlockIndex < diffBlocks.size() && diffBlocks.at(nextBlockIndex).first < sourceIndex)
                    {
                        ++nextBlockIndex;
                    }
                    if (nextBlockIndex < diffBlocks.size() && diffBlocks.at(nextBlockIndex).first == sourceIndex)
                    {
                        m_diffBlockIndexes.append(renderLines.size());
                    }

                    const DiffLine &diff = m_allDiffs.at(sourceIndex);
                    renderLines.append({diff, colorForStatus(diff.status, true), colorForStatus(diff.status, false), false});
                    leftLines.append(diff.textA);
                    rightLines.append(diff.textB);
                    m_renderedDiffs.append(diff);
                }
            }

            if (!showOnlyDiffs)
            {
                m_renderedDiffs = m_allDiffs;
                m_diffBlockIndexes.clear();
                for (const auto &block : diffBlocks)
                {
                    m_diffBlockIndexes.append(block.first);
                }
                leftLines.clear();
                rightLines.clear();
                renderLines.clear();
                renderLines.reserve(m_allDiffs.size());
                for (const DiffLine &diff : m_allDiffs)
                {
                    renderLines.append({diff, colorForStatus(diff.status, true), colorForStatus(diff.status, false), false});
                    leftLines.append(diff.textA);
                    rightLines.append(diff.textB);
                }
            }
        }

        // 单文件预览模式提示
        if (!hasBothFiles) {
            m_summaryLabel->setStyleSheet(QStringLiteral("color: #E8A820; font-weight: bold;"));
            m_summaryLabel->setText(tr("⚠ 预览模式 — 已加载 %1，请加载另一侧文件后进行完整比较")
                .arg(m_leftFile.loaded ? tr("左侧文件") : tr("右侧文件")));
        }

        // 分块渐进式渲染：保存待渲染数据，由定时器驱动逐块渲染
        m_pendingRenderLines = renderLines;
        m_pendingLineLists = {leftLines, rightLines};
        m_renderChunkIndex = 0;
        m_summaryLabel->setText(tr("正在渲染... 0%"));
        m_renderTimer->start(0);
    }

    void FileComparePage::clearComparison()
    {
        m_leftFile = LoadedFile();
        m_rightFile = LoadedFile();
        m_allDiffs.clear();
        m_renderedDiffs.clear();
        m_diffBlockIndexes.clear();
        m_currentDiffIndex = -1;
        m_addedCount = 0;
        m_removedCount = 0;
        m_modifiedCount = 0;
        m_equalCount = 0;
        m_diffBlockCount = 0;
        m_expandedBlocks.clear();
        m_collapsedBlockMap.clear();
        m_renderTimer->stop();
        m_pendingRenderLines.clear();
        m_pendingLineLists.first.clear();
        m_pendingLineLists.second.clear();
        m_leftEditor->clear();
        m_rightEditor->clear();
        m_minimap->setDiffData({});
        updateFileLabels();
        updateVisibleState();
        updateStatusCards();
        updateActionStates();
    }

    void FileComparePage::swapFiles()
    {
        std::swap(m_leftFile, m_rightFile);
        // 直接翻转现有 diff 结果，避免重新 diff 和丢失导航位置
        if (!m_allDiffs.isEmpty()) {
            for (auto &diff : m_allDiffs) {
                std::swap(diff.textA, diff.textB);
                if (diff.status == DiffLine::Added) {
                    diff.status = DiffLine::Removed;
                } else if (diff.status == DiffLine::Removed) {
                    diff.status = DiffLine::Added;
                }
                // Modified 不需要翻转 status，但 byteDiffs 已在两侧都画，不用动
            }
            // 交换统计数据
            std::swap(m_addedCount, m_removedCount);
            updateFileLabels();
            renderDiffs();
            if (m_currentDiffIndex >= 0 && !m_diffBlockIndexes.isEmpty()) {
                navigateDiff(0); // 跳转到当前位置
            }
        } else {
            updateFileLabels();
            compareFiles();
        }
    }

    void FileComparePage::dragEnterEvent(QDragEnterEvent *event)
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        }
    }

    void FileComparePage::dragMoveEvent(QDragMoveEvent *event)
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
            // 高亮提示：根据位置显示将放入左侧还是右侧
            bool toLeft = event->position().toPoint().x() < width() / 2;
            event->setDropAction(toLeft ? Qt::CopyAction : Qt::MoveAction);
        }
    }

    void FileComparePage::dropEvent(QDropEvent *event)
    {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (urls.isEmpty()) return;

        // 根据鼠标释放位置判断：左半 → 左侧文件，右半 → 右侧文件
        bool toLeft = event->position().toPoint().x() < width() / 2;

        loadFileFromPath(urls.at(0).toLocalFile(), toLeft);
        if (urls.size() > 1) {
            // 多个文件同时拖入：第二个放入另一侧
            loadFileFromPath(urls.at(1).toLocalFile(), !toLeft);
        }
    }

    void FileComparePage::navigateDiff(int delta)
    {
        if (m_diffBlockIndexes.isEmpty())
        {
            return;
        }

        if (m_currentDiffIndex < 0)
        {
            m_currentDiffIndex = delta > 0 ? 0 : m_diffBlockIndexes.size() - 1;
        }
        else
        {
            m_currentDiffIndex = (m_currentDiffIndex + delta + m_diffBlockIndexes.size()) % m_diffBlockIndexes.size();
        }

        const int lineIndex = m_diffBlockIndexes.at(m_currentDiffIndex);
        QTextBlock blockL = m_leftEditor->document()->findBlockByNumber(lineIndex);
        QTextBlock blockR = m_rightEditor->document()->findBlockByNumber(lineIndex);
        if (blockL.isValid())
        {
            m_leftEditor->setTextCursor(QTextCursor(blockL));
            m_leftEditor->centerCursor();
        }
        if (blockR.isValid())
        {
            m_rightEditor->setTextCursor(QTextCursor(blockR));
            m_rightEditor->centerCursor();
        }

        updateStatusCards();
    }

    void FileComparePage::updateFileLabels()
    {
        m_leftFileLabel->setText(tr("左侧：%1").arg(fileCaption(m_leftFile, tr("未选择文件"))));
        m_rightFileLabel->setText(tr("右侧：%1").arg(fileCaption(m_rightFile, tr("未选择文件"))));
        m_leftPathLabel->setText(m_leftFile.loaded ? m_leftFile.path : tr("路径：--"));
        m_rightPathLabel->setText(m_rightFile.loaded ? m_rightFile.path : tr("路径：--"));
        m_leftFileLabel->setToolTip(m_leftFile.path);
        m_rightFileLabel->setToolTip(m_rightFile.path);
        m_leftPathLabel->setToolTip(m_leftFile.path);
        m_rightPathLabel->setToolTip(m_rightFile.path);
    }

    void FileComparePage::updateVisibleState()
    {
        const bool hasAnyFile = m_leftFile.loaded || m_rightFile.loaded;
        m_contentStack->setCurrentIndex(hasAnyFile ? 1 : 0);
    }

    void FileComparePage::updateStatusCards()
    {
        auto setFileCard = [](IndustrialStatusCard *card, const LoadedFile &file, const QString &emptyText)
        {
            if (!file.loaded)
            {
                card->setStatus(QObject::tr("未选择"), QColor(QStringLiteral("#5F7890")));
                card->setDetail(emptyText);
                return;
            }

            card->setStatus(QFileInfo(file.path).fileName(), QColor(QStringLiteral("#2F7FB5")));
            card->setDetail(QObject::tr("%1 | %2\n%3")
                                .arg(formatBytes(file.size), fileKindLabel(file.binary), file.path));
        };

        setFileCard(m_leftStatusCard, m_leftFile, tr("等待载入左侧文件"));
        setFileCard(m_rightStatusCard, m_rightFile, tr("等待载入右侧文件"));

        if (!m_leftFile.loaded && !m_rightFile.loaded)
        {
            m_compareStatusCard->setStatus(tr("等待开始"), QColor(QStringLiteral("#5F7890")));
            m_compareStatusCard->setDetail(tr("先选择左右文件，再进行比较。"));
            m_summaryLabel->setText(tr("请选择两个文件进行比较"));
            return;
        }

        if (!m_leftFile.loaded || !m_rightFile.loaded)
        {
            const int loadedCount = (m_leftFile.loaded ? 1 : 0) + (m_rightFile.loaded ? 1 : 0);
            m_compareStatusCard->setStatus(tr("等待另一侧文件"), QColor(QStringLiteral("#2F7FB5")));
            m_compareStatusCard->setDetail(tr("当前已加载 %1 个文件，可先预览内容。再选择另一侧文件后开始比较。").arg(loadedCount));
            m_summaryLabel->setText(tr("已加载 %1 个文件，等待另一侧文件").arg(loadedCount));
            return;
        }

        if (m_diffBlockCount == 0)
        {
            m_compareStatusCard->setStatus(tr("无差异"), QColor(QStringLiteral("#2C8C5A")));
            m_compareStatusCard->setDetail(tr("%1 行内容一致").arg(m_equalCount));
            m_summaryLabel->setText(tr("无差异，%1 行相同").arg(m_equalCount));
            return;
        }

        QString detail = tr("%1 个变更块 | 新增 %2 行 删除 %3 行 修改 %4 行")
                             .arg(m_diffBlockCount)
                             .arg(m_addedCount)
                             .arg(m_removedCount)
                             .arg(m_modifiedCount);
        if (m_showOnlyDiffsCheckBox->isChecked())
        {
            detail += tr(" | 已折叠相同内容");
        }

        m_compareStatusCard->setStatus(tr("发现差异"), QColor(QStringLiteral("#B8801E")));
        m_compareStatusCard->setDetail(detail);

        if (m_currentDiffIndex >= 0 && m_currentDiffIndex < m_diffBlockIndexes.size())
        {
            m_summaryLabel->setText(tr("正在查看第 %1/%2 个变更块 | 新增 %3 行 删除 %4 行 修改 %5 行")
                                        .arg(m_currentDiffIndex + 1)
                                        .arg(m_diffBlockIndexes.size())
                                        .arg(m_addedCount)
                                        .arg(m_removedCount)
                                        .arg(m_modifiedCount));
        }
        else
        {
            m_summaryLabel->setText(tr("共 %1 个变更块 | 新增 %2 行 删除 %3 行 修改 %4 行")
                                        .arg(m_diffBlockCount)
                                        .arg(m_addedCount)
                                        .arg(m_removedCount)
                                        .arg(m_modifiedCount));
        }
    }

    QList<QPair<int, int>> FileComparePage::collectDiffBlocks(const QList<DiffLine> &diffs) const
    {
        QList<QPair<int, int>> blocks;
        int blockStart = -1;

        for (int index = 0; index < diffs.size(); ++index)
        {
            if (diffs.at(index).status != DiffLine::Equal)
            {
                if (blockStart < 0)
                {
                    blockStart = index;
                }
                continue;
            }

            if (blockStart >= 0)
            {
                blocks.append({blockStart, index});
                blockStart = -1;
            }
        }

        if (blockStart >= 0)
        {
            blocks.append({blockStart, diffs.size()});
        }

        return blocks;
    }

    void FileComparePage::updateActionStates()
    {
        const bool hasAnyFile = m_leftFile.loaded || m_rightFile.loaded;
        const bool hasBothFiles = m_leftFile.loaded && m_rightFile.loaded;
        const bool hasDiffs = !m_diffBlockIndexes.isEmpty();

        m_btnCompare->setEnabled(hasBothFiles);
        m_btnSwap->setEnabled(hasAnyFile);
        m_btnClear->setEnabled(hasAnyFile || !m_allDiffs.isEmpty());
        m_btnPrevDiff->setEnabled(hasDiffs);
        m_btnNextDiff->setEnabled(hasDiffs);
        m_ignoreWhitespaceCheckBox->setEnabled(hasBothFiles);
        m_ignoreCaseCheckBox->setEnabled(hasBothFiles);
        m_showOnlyDiffsCheckBox->setEnabled(hasDiffs);
        m_encodingCombo->setEnabled(hasAnyFile);

        // P4.1: 接受按钮（需双侧文件且有当前差异块）
        const bool hasCurrentBlock = m_currentDiffIndex >= 0 && m_currentDiffIndex < m_diffBlockIndexes.size();
        m_btnAcceptLeft->setEnabled(hasBothFiles && hasCurrentBlock);
        m_btnAcceptRight->setEnabled(hasBothFiles && hasCurrentBlock);

        // P4.5: 导出按钮
        m_btnExport->setEnabled(!m_allDiffs.isEmpty());

        // P5.2: 最近按钮
        m_btnRecent->setEnabled(m_recentRecordManager != nullptr);
    }

    void FileComparePage::renderNextChunk()
    {
        constexpr int kChunkSize = 200;
        const int totalLines = m_pendingRenderLines.size();

        // 防御：若渲染期间 clearComparison() 已清空数据，直接中止
        if (totalLines == 0 || m_renderChunkIndex >= totalLines) {
            // 渲染完成，构建 ExtraSelections
            QList<QTextEdit::ExtraSelection> leftSelections;
            QList<QTextEdit::ExtraSelection> rightSelections;
            QTextBlock leftBlock = m_leftEditor->document()->firstBlock();
            QTextBlock rightBlock = m_rightEditor->document()->firstBlock();
            for (const RenderLine &line : m_pendingRenderLines) {
                if (line.leftColor.isValid()) {
                    QTextEdit::ExtraSelection sel;
                    sel.format.setBackground(line.leftColor);
                    sel.format.setProperty(QTextFormat::FullWidthSelection, true);
                    sel.cursor = QTextCursor(leftBlock);
                    sel.cursor.clearSelection();
                    leftSelections.append(sel);
                }
                if (line.rightColor.isValid()) {
                    QTextEdit::ExtraSelection sel;
                    sel.format.setBackground(line.rightColor);
                    sel.format.setProperty(QTextFormat::FullWidthSelection, true);
                    sel.cursor = QTextCursor(rightBlock);
                    sel.cursor.clearSelection();
                    rightSelections.append(sel);
                }
                if (!line.placeholder && line.diff.status == DiffLine::Modified) {
                    if (!line.diff.textA.isEmpty()) {
                        QTextEdit::ExtraSelection inlineSel;
                        inlineSel.format.setBackground(modifiedInlineColor());
                        QPair<int, int> range = changedRange(line.diff.textA, line.diff.textB);
                        inlineSel.cursor = QTextCursor(leftBlock);
                        inlineSel.cursor.setPosition(leftBlock.position() + range.first);
                        inlineSel.cursor.setPosition(leftBlock.position() + range.first + range.second, QTextCursor::KeepAnchor);
                        leftSelections.append(inlineSel);
                    }
                    if (!line.diff.textB.isEmpty()) {
                        QTextEdit::ExtraSelection inlineSel;
                        inlineSel.format.setBackground(modifiedInlineColor());
                        QPair<int, int> range = changedRange(line.diff.textB, line.diff.textA);
                        inlineSel.cursor = QTextCursor(rightBlock);
                        inlineSel.cursor.setPosition(rightBlock.position() + range.first);
                        inlineSel.cursor.setPosition(rightBlock.position() + range.first + range.second, QTextCursor::KeepAnchor);
                        rightSelections.append(inlineSel);
                    }
                    // 字节级差异（二进制模式，用更深的橙色高亮变化的字节）
                    for (const auto &byteDiff : line.diff.byteDiffs) {
                        QTextEdit::ExtraSelection byteSel;
                        byteSel.format.setBackground(QColor(QStringLiteral("#E8A820"))); // 深橙色
                        byteSel.cursor = QTextCursor(leftBlock);
                        byteSel.cursor.setPosition(leftBlock.position() + byteDiff.offset);
                        byteSel.cursor.setPosition(leftBlock.position() + byteDiff.offset + byteDiff.length, QTextCursor::KeepAnchor);
                        leftSelections.append(byteSel);

                        QTextEdit::ExtraSelection byteSelR;
                        byteSelR.format.setBackground(QColor(QStringLiteral("#E8A820")));
                        byteSelR.cursor = QTextCursor(rightBlock);
                        byteSelR.cursor.setPosition(rightBlock.position() + byteDiff.offset);
                        byteSelR.cursor.setPosition(rightBlock.position() + byteDiff.offset + byteDiff.length, QTextCursor::KeepAnchor);
                        rightSelections.append(byteSelR);
                    }
                }
                leftBlock = leftBlock.next();
                rightBlock = rightBlock.next();
            }
            m_leftEditor->setExtraSelections(leftSelections);
            m_rightEditor->setExtraSelections(rightSelections);
            m_minimap->setDiffData(m_renderedDiffs);

            updateVisibleState();
            updateStatusCards();
            updateActionStates();

            // 清除 pending 数据以释放内存
            m_pendingRenderLines.clear();
            m_pendingLineLists.first.clear();
            m_pendingLineLists.second.clear();
            m_expandingFold = false;  // P4.2: 渲染完成，恢复折叠展开检测
            return;
        }

        int endIndex = std::min(m_renderChunkIndex + kChunkSize, totalLines);

        QTextCursor leftCursor(m_leftEditor->document());
        leftCursor.movePosition(QTextCursor::End);
        QTextCursor rightCursor(m_rightEditor->document());
        rightCursor.movePosition(QTextCursor::End);

        bool firstBlock = (m_renderChunkIndex == 0);

        for (int i = m_renderChunkIndex; i < endIndex; ++i) {
            if (!firstBlock || i > m_renderChunkIndex) {
                leftCursor.insertBlock();
                rightCursor.insertBlock();
            }
            // 防御：clearComparison() 可能在渲染期间清空 pending 数据
            if (i < m_pendingLineLists.first.size() && i < m_pendingLineLists.second.size()) {
                leftCursor.insertText(m_pendingLineLists.first.at(i));
                rightCursor.insertText(m_pendingLineLists.second.at(i));
            }
        }

        m_renderChunkIndex = endIndex;

        int percent = totalLines > 0 ? (m_renderChunkIndex * 100 / totalLines) : 100;
        m_summaryLabel->setText(tr("正在渲染... %1%").arg(percent));

        m_renderTimer->start(0);
    }

    void FileComparePage::syncScrollBar(QScrollBar *source, QScrollBar *target)
    {
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

    void FileComparePage::acceptCurrentBlock(bool fromLeft)
    {
        if (m_currentDiffIndex < 0 || m_currentDiffIndex >= m_diffBlockIndexes.size()) return;
        if (!m_leftFile.loaded || !m_rightFile.loaded) return;

        // 找到当前差异块的范围
        const int blockStart = m_diffBlockIndexes.at(m_currentDiffIndex);
        int blockEnd = blockStart;
        while (blockEnd < m_allDiffs.size() && m_allDiffs.at(blockEnd).status != DiffLine::Equal)
        {
            ++blockEnd;
        }

        // 修改文件行数据
        for (int i = blockStart; i < blockEnd; ++i)
        {
            DiffLine &diff = m_allDiffs[i];
            if (fromLeft)
            {
                diff.textB = diff.textA;
            }
            else
            {
                diff.textA = diff.textB;
            }
            diff.status = DiffLine::Equal;
        }

        // 合并相邻 Equal 行并重新渲染
        renderDiffs();
    }

    void FileComparePage::setRecentRecordManager(RecentRecordManager *manager)
    {
        m_recentRecordManager = manager;
    }

    void FileComparePage::refreshRecentMenu()
    {
        if (!m_recentRecordManager || !m_btnRecent) return;

        QMenu *menu = new QMenu(this);
        QVariantList pairs = m_recentRecordManager->recentComparePairs();

        if (pairs.isEmpty())
        {
            menu->addAction(tr("(无最近记录)"))->setEnabled(false);
        }
        else
        {
            for (const QVariant &v : pairs)
            {
                QVariantMap m = v.toMap();
                QString label = QStringLiteral("%1 ↔ %2").arg(m.value(QStringLiteral("leftName")).toString(),
                                                               m.value(QStringLiteral("rightName")).toString());
                QString leftPath = m.value(QStringLiteral("leftPath")).toString();
                QString rightPath = m.value(QStringLiteral("rightPath")).toString();
                menu->addAction(label, this, [this, leftPath, rightPath]()
                                {
                    loadFileFromPath(leftPath, true);
                    loadFileFromPath(rightPath, false); });
            }
        }

        QMenu *oldMenu = m_btnRecent->menu();
        m_btnRecent->setMenu(menu);
        // QPushButton::setMenu() 接管了 oldMenu 的所有权并会自动删除它
        // 不能再手动 delete，否则造成 double-free
        Q_UNUSED(oldMenu)
    }

}
