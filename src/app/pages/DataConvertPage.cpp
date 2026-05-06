#include "pages/DataConvertPage.h"

#include "widgets/DataConvertWidget.h"

#include <QLabel>
#include <QVBoxLayout>

namespace est
{

    DataConvertPage::DataConvertPage(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("dataConvertPage"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(24, 24, 24, 24);
        rootLayout->setSpacing(16);

        auto *titleLabel = new QLabel(tr("数据转换"), this);
        titleLabel->setObjectName(QStringLiteral("dataConvertTitle"));

        auto *subtitleLabel = new QLabel(
            tr("提供字符串、HEX、二进制、Base64 与十进制字节列表之间的即时转换。"),
            this);
        subtitleLabel->setObjectName(QStringLiteral("dataConvertSubtitle"));
        subtitleLabel->setWordWrap(true);

        m_dataConvertWidget = new DataConvertWidget(this);

        rootLayout->addWidget(titleLabel);
        rootLayout->addWidget(subtitleLabel);
        rootLayout->addWidget(m_dataConvertWidget, 1);

        connect(m_dataConvertWidget, &DataConvertWidget::statusMessageGenerated,
                this, &DataConvertPage::statusMessageGenerated);
    }

} // namespace est
