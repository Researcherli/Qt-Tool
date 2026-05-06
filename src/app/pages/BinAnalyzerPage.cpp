#include "pages/BinAnalyzerPage.h"

#include "widgets/BinAnalyzerWidget.h"

#include <QLabel>
#include <QVBoxLayout>

namespace est
{

    BinAnalyzerPage::BinAnalyzerPage(ICore *core, QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("binAnalyzerPage"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(24, 24, 24, 24);
        rootLayout->setSpacing(16);

        auto *titleLabel = new QLabel(tr("BIN 文件分析"), this);
        titleLabel->setObjectName(QStringLiteral("binAnalyzerTitle"));

        auto *subtitleLabel = new QLabel(
            tr("提供 BIN 文件导入、HEX 查看、搜索与字符串提取。"),
            this);
        subtitleLabel->setObjectName(QStringLiteral("binAnalyzerSubtitle"));
        subtitleLabel->setWordWrap(true);

        m_binAnalyzerWidget = new BinAnalyzerWidget(core, this);

        rootLayout->addWidget(titleLabel);
        rootLayout->addWidget(subtitleLabel);
        rootLayout->addWidget(m_binAnalyzerWidget, 1);

        connect(m_binAnalyzerWidget, &BinAnalyzerWidget::currentFileChanged,
                this, &BinAnalyzerPage::currentFileChanged);
        connect(m_binAnalyzerWidget, &BinAnalyzerWidget::statusMessageGenerated,
                this, &BinAnalyzerPage::statusMessageGenerated);
        connect(m_binAnalyzerWidget, &BinAnalyzerWidget::recentRecordsChanged,
                this, &BinAnalyzerPage::recentRecordsChanged);
    }

    void BinAnalyzerPage::openFileDialog()
    {
        if (m_binAnalyzerWidget != nullptr)
        {
            m_binAnalyzerWidget->openFileDialog();
        }
    }

} // namespace est
