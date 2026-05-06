#include "pages/ModulePlaceholderPage.h"

#include <QLabel>
#include <QVBoxLayout>

namespace est
{

    ModulePlaceholderPage::ModulePlaceholderPage(const QString &title,
                                                 const QString &description,
                                                 const QString &badgeText,
                                                 QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("modulePlaceholderPage"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(32, 32, 32, 32);
        rootLayout->setSpacing(12);

        auto *badgeLabel = new QLabel(badgeText, this);
        badgeLabel->setObjectName(QStringLiteral("moduleBadge"));

        auto *titleLabel = new QLabel(title, this);
        titleLabel->setObjectName(QStringLiteral("moduleTitle"));

        auto *descLabel = new QLabel(description, this);
        descLabel->setObjectName(QStringLiteral("moduleDescription"));
        descLabel->setWordWrap(true);

        rootLayout->addWidget(badgeLabel, 0, Qt::AlignLeft);
        rootLayout->addWidget(titleLabel);
        rootLayout->addWidget(descLabel);
        rootLayout->addStretch(1);
    }

} // namespace est
