#include "pages/BinAnalyzerPage.h"

#include "widgets/BinAnalyzerWidget.h"

#include <QVBoxLayout>

namespace est
{

    BinAnalyzerPage::BinAnalyzerPage(ICore *core, QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("binAnalyzerPage"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(24, 24, 24, 24);
        rootLayout->setSpacing(0);

        m_binAnalyzerWidget = new BinAnalyzerWidget(core, this);

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
