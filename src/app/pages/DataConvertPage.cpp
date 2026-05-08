#include "pages/DataConvertPage.h"

#include "widgets/DataConvertWidget.h"

#include <QVBoxLayout>

namespace est
{

    DataConvertPage::DataConvertPage(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("dataConvertPage"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(16, 16, 16, 16);
        rootLayout->setSpacing(0);

        m_dataConvertWidget = new DataConvertWidget(this);

        rootLayout->addWidget(m_dataConvertWidget, 1);

        connect(m_dataConvertWidget, &DataConvertWidget::statusMessageGenerated,
                this, &DataConvertPage::statusMessageGenerated);
    }

} // namespace est
