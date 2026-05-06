#include "widgets/IndustrialStatusCard.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace est
{

    IndustrialStatusCard::IndustrialStatusCard(const QString &title, QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("industrialStatusCard"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(18, 18, 18, 18);
        rootLayout->setSpacing(10);

        auto *titleLayout = new QHBoxLayout();
        titleLayout->setContentsMargins(0, 0, 0, 0);
        titleLayout->setSpacing(10);

        m_indicator = new QFrame(this);
        m_indicator->setFixedSize(12, 12);
        m_indicator->setObjectName(QStringLiteral("statusIndicator"));

        m_titleLabel = new QLabel(title, this);
        m_titleLabel->setObjectName(QStringLiteral("statusCardTitle"));

        titleLayout->addWidget(m_indicator, 0, Qt::AlignVCenter);
        titleLayout->addWidget(m_titleLabel, 1);

        m_statusLabel = new QLabel(tr("未连接"), this);
        m_statusLabel->setObjectName(QStringLiteral("statusCardValue"));

        m_detailLabel = new QLabel(tr("等待状态更新"), this);
        m_detailLabel->setObjectName(QStringLiteral("statusCardDetail"));
        m_detailLabel->setWordWrap(true);

        rootLayout->addLayout(titleLayout);
        rootLayout->addWidget(m_statusLabel);
        rootLayout->addWidget(m_detailLabel);

        setStatus(tr("未连接"), QColor(QStringLiteral("#5F7890")));
    }

    void IndustrialStatusCard::setStatus(const QString &text, const QColor &color)
    {
        m_statusLabel->setText(text);
        m_indicator->setStyleSheet(QStringLiteral(
                                       "background-color:%1; border-radius:6px; border:1px solid %2;")
                                       .arg(color.name(), color.darker(150).name()));
    }

    void IndustrialStatusCard::setDetail(const QString &text)
    {
        m_detailLabel->setText(text);
    }

} // namespace est
