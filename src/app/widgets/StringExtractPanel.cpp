#include "widgets/StringExtractPanel.h"

#include "services/AppPaths.h"

#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

namespace est
{

    StringExtractPanel::StringExtractPanel(QWidget *parent)
        : QWidget(parent)
    {
        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(8);

        auto *toolLayout = new QHBoxLayout();
        toolLayout->addWidget(new QLabel(tr("最小长度"), this));

        m_minLengthSpinBox = new QSpinBox(this);
        m_minLengthSpinBox->setRange(2, 64);
        m_minLengthSpinBox->setValue(4);

        auto *refreshButton = new QPushButton(tr("重新提取"), this);
        m_filterEdit = new QLineEdit(this);
        m_filterEdit->setObjectName(QStringLiteral("stringFilterEdit"));
        m_filterEdit->setMinimumWidth(280);
        m_filterEdit->setPlaceholderText(tr("按关键字过滤字符串"));

        toolLayout->addWidget(m_minLengthSpinBox);
        toolLayout->addWidget(refreshButton);
        toolLayout->addWidget(m_filterEdit, 1);

        m_tableWidget = new QTableWidget(this);
        m_tableWidget->setColumnCount(4);
        m_tableWidget->setHorizontalHeaderLabels({tr("偏移"), tr("长度"), tr("编码"), tr("内容")});
        m_tableWidget->horizontalHeader()->setStretchLastSection(true);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_tableWidget->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
        m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_tableWidget->setAlternatingRowColors(true);

        rootLayout->addLayout(toolLayout);
        rootLayout->addWidget(m_tableWidget, 1);

        connect(refreshButton, &QPushButton::clicked, this, &StringExtractPanel::refreshResults);
        connect(m_filterEdit, &QLineEdit::textChanged, this, [this](const QString &) { applyFilter(); });
        connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
            if (row < 0 || row >= m_entries.size())
            {
                return;
            }
            const ExtractedStringEntry &entry = m_entries.at(row);
            emit offsetActivated(entry.offset, entry.length);
        });
    }

    void StringExtractPanel::setData(const QByteArray &data)
    {
        m_data = data;
        refreshResults();
    }

    void StringExtractPanel::clear()
    {
        m_data.clear();
        m_entries.clear();
        m_tableWidget->setRowCount(0);
    }

    void StringExtractPanel::exportResults()
    {
        if (m_entries.isEmpty())
        {
            emit statusMessageGenerated(tr("当前没有可导出的字符串结果"));
            return;
        }

        const QString filePath = QFileDialog::getSaveFileName(
            this,
            tr("导出字符串结果"),
            AppPaths::exportFilePath(QStringLiteral("bin_strings.csv")),
            tr("CSV 文件 (*.csv);;文本文件 (*.txt)"));
        if (filePath.isEmpty())
        {
            return;
        }
        if (AppPaths::isDriveCPath(filePath))
        {
            emit statusMessageGenerated(tr("保存路径不能在 C 盘，请选择软件目录下的数据文件夹"));
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
        stream << "offset,length,encoding,content\n";
        for (const ExtractedStringEntry &entry : m_entries)
        {
            QString safeContent = entry.content;
            stream << QStringLiteral("0x%1,%2,%3,\"%4\"\n")
                          .arg(entry.offset, 8, 16, QLatin1Char('0'))
                          .arg(entry.length)
                          .arg(entry.encoding)
                          .arg(safeContent.replace(QStringLiteral("\""), QStringLiteral("\"\"")));
        }

        emit statusMessageGenerated(tr("字符串结果已导出到：%1").arg(filePath));
    }

    void StringExtractPanel::refreshResults()
    {
        if (m_data.isEmpty())
        {
            clear();
            return;
        }

        const ByteFormatService::TextEncoding encoding = ByteFormatService::TextEncoding::ASCII;
        m_entries = StringExtractor::extract(m_data, m_minLengthSpinBox->value(), encoding);

        m_tableWidget->setRowCount(m_entries.size());
        for (int row = 0; row < m_entries.size(); ++row)
        {
            const ExtractedStringEntry &entry = m_entries.at(row);
            m_tableWidget->setItem(row, 0, new QTableWidgetItem(
                QStringLiteral("0x%1").arg(entry.offset, 8, 16, QLatin1Char('0')).toUpper()));
            m_tableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(entry.length)));
            m_tableWidget->setItem(row, 2, new QTableWidgetItem(entry.encoding));
            m_tableWidget->setItem(row, 3, new QTableWidgetItem(entry.content));
        }

        applyFilter();
        emit statusMessageGenerated(tr("字符串提取完成，共 %1 条结果").arg(m_entries.size()));
    }

    void StringExtractPanel::applyFilter()
    {
        const QString filter = m_filterEdit->text().trimmed();
        for (int row = 0; row < m_tableWidget->rowCount(); ++row)
        {
            const bool match = filter.isEmpty()
                               || m_entries.at(row).content.contains(filter, Qt::CaseInsensitive);
            m_tableWidget->setRowHidden(row, !match);
        }
    }

} // namespace est
