#include "widgets/BinAnalyzerWidget.h"

#include "plugin/ICore.h"
#include "services/BinSearchService.h"
#include "services/ByteChecksumService.h"
#include "services/ByteDataInspectorService.h"
#include "services/ByteFormatService.h"
#include "services/RecentRecordManager.h"
#include "widgets/HexViewerWidget.h"
#include "widgets/SearchBarWidget.h"
#include "widgets/StringExtractPanel.h"

#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

namespace est
{

    BinAnalyzerWidget::BinAnalyzerWidget(ICore *core, QWidget *parent)
        : QWidget(parent)
        , m_recentRecordManager(core->recentRecordManager())
    {
        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(12);

        setAcceptDrops(true);

        auto *fileLayout = new QHBoxLayout();
        auto *openButton = new QPushButton(tr("导入 BIN"), this);
        auto *reloadButton = new QPushButton(tr("重新加载"), this);
        auto *exportStringsButton = new QPushButton(tr("导出字符串"), this);
        auto *exportHexButton = new QPushButton(tr("导出 HEX"), this);
        m_filePathEdit = new QLineEdit(this);
        m_filePathEdit->setReadOnly(true);
        m_filePathEdit->setPlaceholderText(tr("请先选择 BIN 文件"));
        m_fileSizeLabel = new QLabel(tr("大小：0 B"), this);

        fileLayout->addWidget(openButton);
        fileLayout->addWidget(reloadButton);
        fileLayout->addWidget(m_filePathEdit, 1);
        fileLayout->addWidget(m_fileSizeLabel);
        fileLayout->addWidget(exportStringsButton);
        fileLayout->addWidget(exportHexButton);

        m_searchBar = new SearchBarWidget(this);

        auto *splitter = new QSplitter(Qt::Horizontal, this);
        m_hexViewer = new HexViewerWidget(splitter);

        auto *analysisTabs = new QTabWidget(splitter);
        m_stringPanel = new StringExtractPanel(analysisTabs);

        auto *inspectorPanel = new QWidget(analysisTabs);
        auto *inspectorLayout = new QVBoxLayout(inspectorPanel);
        inspectorLayout->setContentsMargins(0, 0, 0, 0);
        inspectorLayout->setSpacing(8);

        auto *inspectorToolLayout = new QHBoxLayout();
        inspectorToolLayout->addWidget(new QLabel(tr("字节序"), inspectorPanel));
        m_endianCombo = new QComboBox(inspectorPanel);
        m_endianCombo->addItem(tr("Little Endian"), QStringLiteral("little"));
        m_endianCombo->addItem(tr("Big Endian"), QStringLiteral("big"));
        inspectorToolLayout->addWidget(m_endianCombo);
        inspectorToolLayout->addStretch(1);

        m_inspectorTable = createKeyValueTable(QStringLiteral("binInspectorTable"));
        inspectorLayout->addLayout(inspectorToolLayout);
        inspectorLayout->addWidget(m_inspectorTable, 1);

        auto *checksumPanel = new QWidget(analysisTabs);
        auto *checksumLayout = new QVBoxLayout(checksumPanel);
        checksumLayout->setContentsMargins(0, 0, 0, 0);
        m_checksumTable = createKeyValueTable(QStringLiteral("binChecksumTable"));
        checksumLayout->addWidget(m_checksumTable);

        analysisTabs->addTab(m_stringPanel, tr("字符串"));
        analysisTabs->addTab(inspectorPanel, tr("当前位置解析"));
        analysisTabs->addTab(checksumPanel, tr("校验"));
        splitter->addWidget(m_hexViewer);
        splitter->addWidget(analysisTabs);
        splitter->setStretchFactor(0, 3);
        splitter->setStretchFactor(1, 2);
        splitter->setSizes({820, 480});

        m_detailLabel = new QLabel(tr("当前偏移：0x00000000 | 当前字节：0x00 | ASCII：."), this);
        m_detailLabel->setObjectName(QStringLiteral("binDetailLabel"));

        rootLayout->addLayout(fileLayout);
        rootLayout->addWidget(m_searchBar);
        rootLayout->addWidget(splitter, 1);
        rootLayout->addWidget(m_detailLabel);

        connect(openButton, &QPushButton::clicked, this, &BinAnalyzerWidget::openFileDialog);
        connect(reloadButton, &QPushButton::clicked, this, &BinAnalyzerWidget::reloadFile);
        connect(exportStringsButton, &QPushButton::clicked, m_stringPanel, &StringExtractPanel::exportResults);
        connect(exportHexButton, &QPushButton::clicked, this, &BinAnalyzerWidget::exportHexText);
        connect(m_searchBar, &SearchBarWidget::searchRequested, this, &BinAnalyzerWidget::runSearch);
        connect(m_searchBar, &SearchBarWidget::previousRequested, this, [this]() { goToSearchResult(-1); });
        connect(m_searchBar, &SearchBarWidget::nextRequested, this, [this]() { goToSearchResult(1); });
        connect(m_searchBar, &SearchBarWidget::clearRequested, this, &BinAnalyzerWidget::clearSearch);
        connect(m_hexViewer, &HexViewerWidget::offsetChanged, this, &BinAnalyzerWidget::updateDetail);
        connect(m_endianCombo, &QComboBox::currentIndexChanged, this, [this](int) {
            updateInspector(m_currentOffset);
        });
        connect(m_stringPanel, &StringExtractPanel::offsetActivated, this, [this](qint64 offset, int length) {
            m_hexViewer->highlightRange(offset, length);
        });
        connect(m_stringPanel, &StringExtractPanel::statusMessageGenerated,
                this, &BinAnalyzerWidget::statusMessageGenerated);
    }

    void BinAnalyzerWidget::openFileDialog()
    {
        const QString filePath = QFileDialog::getOpenFileName(
            this,
            tr("选择 BIN 文件"),
            QString(),
            tr("BIN 文件 (*.bin);;所有文件 (*.*)"));
        if (!filePath.isEmpty())
        {
            loadFile(filePath);
        }
    }

    void BinAnalyzerWidget::loadFile(const QString &filePath)
    {
        QString errorMessage;
        if (!m_loader.loadFile(filePath, &errorMessage))
        {
            emit statusMessageGenerated(errorMessage);
            return;
        }

        m_filePathEdit->setText(filePath);
        m_fileSizeLabel->setText(tr("大小：%1").arg(fileSizeText(m_loader.fileSize())));
        m_hexViewer->setData(m_loader.data());
        m_stringPanel->setData(m_loader.data());
        updateInspector(0);
        updateChecksum();
        m_searchResults.clear();
        m_currentSearchIndex = -1;
        m_searchMatchLength = 1;
        m_searchBar->setResultCount(0);

        m_recentRecordManager->addBinFile(filePath, m_loader.fileSize());
        emit currentFileChanged(tr("文件：%1").arg(QFileInfo(filePath).fileName()));
        emit statusMessageGenerated(tr("已加载 BIN 文件：%1").arg(QFileInfo(filePath).fileName()));
        emit recentRecordsChanged();
    }

    void BinAnalyzerWidget::reloadFile()
    {
        QString errorMessage;
        if (!m_loader.reload(&errorMessage))
        {
            emit statusMessageGenerated(errorMessage);
            return;
        }

        m_hexViewer->setData(m_loader.data());
        m_stringPanel->setData(m_loader.data());
        updateInspector(m_currentOffset >= 0 ? m_currentOffset : 0);
        updateChecksum();
        emit statusMessageGenerated(tr("BIN 文件已重新加载"));
    }

    void BinAnalyzerWidget::exportHexText()
    {
        if (!m_loader.isLoaded())
        {
            emit statusMessageGenerated(tr("当前没有可导出的 BIN 数据"));
            return;
        }

        const QString filePath = QFileDialog::getSaveFileName(this, tr("导出 HEX 文本"), QStringLiteral("bin_hex.txt"), tr("文本文件 (*.txt)"));
        if (filePath.isEmpty())
        {
            return;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            emit statusMessageGenerated(tr("导出失败：无法写入 %1").arg(filePath));
            return;
        }

        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);
        stream << ByteFormatService::formatHex(m_loader.data(), QStringLiteral("space"), true);
        emit statusMessageGenerated(tr("HEX 文本已导出到：%1").arg(filePath));
    }

    void BinAnalyzerWidget::runSearch()
    {
        if (!m_loader.isLoaded())
        {
            emit statusMessageGenerated(tr("请先导入 BIN 文件"));
            return;
        }

        QString errorMessage;
        const BinSearchService::SearchQuery query = m_searchBar->currentQuery();
        if (!BinSearchService::search(m_loader.data(), query, &m_searchResults, &errorMessage))
        {
            emit statusMessageGenerated(errorMessage);
            return;
        }
        m_searchMatchLength = searchMatchLength(query);

        m_searchBar->setResultCount(m_searchResults.size());
        m_currentSearchIndex = m_searchResults.isEmpty() ? -1 : 0;

        if (m_searchResults.isEmpty())
        {
            emit statusMessageGenerated(tr("未找到匹配结果"));
            return;
        }

        QString typeText = tr("字符串");
        if (query.type == BinSearchService::SearchType::Hex)
        {
            typeText = QStringLiteral("HEX");
        }
        else if (query.type == BinSearchService::SearchType::Offset)
        {
            typeText = tr("偏移");
        }
        m_recentRecordManager->addSearchKeyword(query.input, typeText);
        emit recentRecordsChanged();

        goToSearchResult(0);
        emit statusMessageGenerated(tr("搜索完成，共 %1 个结果").arg(m_searchResults.size()));
    }

    void BinAnalyzerWidget::goToSearchResult(int step)
    {
        if (m_searchResults.isEmpty())
        {
            return;
        }

        if (step != 0)
        {
            m_currentSearchIndex = (m_currentSearchIndex + step + m_searchResults.size()) % m_searchResults.size();
        }

        const qsizetype offset = m_searchResults.at(m_currentSearchIndex);
        m_hexViewer->highlightMatches(m_searchBar->highlightEnabled() ? m_searchResults : QVector<qsizetype>(),
                                      m_searchBar->highlightEnabled() ? offset : -1,
                                      m_searchMatchLength);
        m_hexViewer->jumpToOffset(offset);
    }

    void BinAnalyzerWidget::clearSearch()
    {
        m_searchResults.clear();
        m_currentSearchIndex = -1;
        m_searchMatchLength = 1;
        m_searchBar->setResultCount(0);
        if (m_loader.isLoaded())
        {
            m_hexViewer->highlightMatches({}, -1, 1);
        }
    }

    void BinAnalyzerWidget::updateDetail(qint64 offset, uchar byteValue, const QString &asciiText)
    {
        m_currentOffset = offset;
        m_detailLabel->setText(
            tr("当前偏移：0x%1 | 当前字节：0x%2 | 十进制：%3 | ASCII：%4")
                .arg(offset, 8, 16, QLatin1Char('0'))
                .arg(byteValue, 2, 16, QLatin1Char('0'))
                .arg(byteValue)
                .arg(asciiText)
                .toUpper());
        updateInspector(offset);
    }

    void BinAnalyzerWidget::updateInspector(qint64 offset)
    {
        if (m_inspectorTable == nullptr)
        {
            return;
        }

        m_inspectorTable->setRowCount(0);
        if (!m_loader.isLoaded() || offset < 0 || offset >= m_loader.data().size())
        {
            return;
        }

        const auto endian = m_endianCombo != nullptr
                                && m_endianCombo->currentData().toString() == QStringLiteral("big")
                            ? ByteDataInspectorService::Endian::Big
                            : ByteDataInspectorService::Endian::Little;
        const QMap<QString, QString> rows = ByteDataInspectorService::inspect(m_loader.data(), offset, endian);
        const QStringList order = {
            QStringLiteral("uint8"),
            QStringLiteral("int8"),
            QStringLiteral("uint16"),
            QStringLiteral("int16"),
            QStringLiteral("uint32"),
            QStringLiteral("int32"),
            QStringLiteral("uint64"),
            QStringLiteral("int64"),
            QStringLiteral("float"),
            QStringLiteral("double"),
        };

        m_inspectorTable->setRowCount(order.size());
        for (int row = 0; row < order.size(); ++row)
        {
            const QString key = order.at(row);
            m_inspectorTable->setItem(row, 0, new QTableWidgetItem(key));
            m_inspectorTable->setItem(row, 1, new QTableWidgetItem(rows.value(key)));
        }
    }

    void BinAnalyzerWidget::updateChecksum()
    {
        if (m_checksumTable == nullptr)
        {
            return;
        }

        m_checksumTable->setRowCount(0);
        if (!m_loader.isLoaded())
        {
            return;
        }

        const QMap<QString, QString> checksums = ByteChecksumService::calculate(m_loader.data());
        m_checksumTable->setRowCount(checksums.size());

        int row = 0;
        for (auto iterator = checksums.cbegin(); iterator != checksums.cend(); ++iterator, ++row)
        {
            m_checksumTable->setItem(row, 0, new QTableWidgetItem(iterator.key()));
            m_checksumTable->setItem(row, 1, new QTableWidgetItem(iterator.value()));
        }
    }

    int BinAnalyzerWidget::searchMatchLength(const BinSearchService::SearchQuery &query) const
    {
        if (query.type == BinSearchService::SearchType::Offset)
        {
            return 1;
        }

        QByteArray pattern;
        QString errorMessage;
        if (query.type == BinSearchService::SearchType::Hex)
        {
            if (!ByteFormatService::parseHex(query.input, &pattern, &errorMessage))
            {
                return 1;
            }
        }
        else
        {
            bool ok = false;
            pattern = ByteFormatService::encodeString(query.input, query.encoding, &ok, &errorMessage);
            if (!ok)
            {
                return 1;
            }
        }

        return qMax(1, pattern.size());
    }

    QTableWidget *BinAnalyzerWidget::createKeyValueTable(const QString &objectName) const
    {
        auto *table = new QTableWidget();
        table->setObjectName(objectName);
        table->setColumnCount(2);
        table->setHorizontalHeaderLabels({tr("项目"), tr("值")});
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        return table;
    }

    QString BinAnalyzerWidget::fileSizeText(qint64 byteCount) const
    {
        if (byteCount < 1024)
        {
            return tr("%1 B").arg(byteCount);
        }
        if (byteCount < 1024 * 1024)
        {
            return tr("%1 KB").arg(byteCount / 1024.0, 0, 'f', 1);
        }
        return tr("%1 MB").arg(byteCount / 1024.0 / 1024.0, 0, 'f', 2);
    }

    void BinAnalyzerWidget::dragEnterEvent(QDragEnterEvent *event)
    {
        if (event->mimeData()->hasUrls()) {
            event->acceptProposedAction();
        }
    }

    void BinAnalyzerWidget::dropEvent(QDropEvent *event)
    {
        const QList<QUrl> urls = event->mimeData()->urls();
        if (urls.isEmpty()) return;

        const QString filePath = urls.first().toLocalFile();
        if (!filePath.isEmpty()) {
            loadFile(filePath);
        }
    }

} // namespace est
