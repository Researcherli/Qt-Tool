#include "pages/HomePage.h"

#include "plugin/ICore.h"
#include "widgets/ModuleIconFactory.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace est
{
    namespace
    {
        QToolButton *createModuleButton(QWidget *parent,
                                        const QString &text,
                                        const QString &iconType,
                                        const QString &objectName)
        {
            auto *button = new QToolButton(parent);
            button->setObjectName(objectName);
            button->setText(text);
            button->setIcon(QIcon(makeModuleIconPixmap(iconType, 96)));
            button->setIconSize(QSize(96, 96));
            button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            button->setMinimumHeight(170);
            button->setCursor(Qt::PointingHandCursor);
            return button;
        }
    }

    HomePage::HomePage(ICore *core, QWidget *parent)
        : QWidget(parent)
    {
        Q_UNUSED(core)

        setObjectName(QStringLiteral("homePage"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(24, 20, 28, 24);
        rootLayout->setSpacing(16);

        auto *homeHeader = new QFrame(this);
        homeHeader->setObjectName(QStringLiteral("homeHeaderCard"));
        homeHeader->setFixedHeight(132);
        auto *homeHeaderLayout = new QHBoxLayout(homeHeader);
        homeHeaderLayout->setContentsMargins(34, 18, 34, 18);
        homeHeaderLayout->setSpacing(18);

        auto *homeIconLabel = new QLabel(homeHeader);
        homeIconLabel->setObjectName(QStringLiteral("homeHeaderIcon"));
        homeIconLabel->setPixmap(makeHomeIconPixmap(88));
        homeIconLabel->setFixedSize(104, 104);
        homeIconLabel->setAlignment(Qt::AlignCenter);

        auto *homeTitleLabel = new QLabel(tr("首页"), homeHeader);
        homeTitleLabel->setObjectName(QStringLiteral("homeHeaderTitle"));
        homeTitleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        homeHeaderLayout->addWidget(homeIconLabel);
        homeHeaderLayout->addWidget(homeTitleLabel, 1);

        rootLayout->addWidget(homeHeader);

        auto *moduleGrid = new QGridLayout();
        moduleGrid->setContentsMargins(0, 0, 0, 0);
        moduleGrid->setHorizontalSpacing(16);
        moduleGrid->setVerticalSpacing(16);

        auto *serialModuleButton = createModuleButton(
            this,
            tr("串口工具"),
            QStringLiteral("serial"),
            QStringLiteral("homeModuleButtonPrimary"));
        auto *convertModuleButton = createModuleButton(
            this,
            tr("数据转换"),
            QStringLiteral("convert"),
            QStringLiteral("homeModuleButton"));
        auto *binModuleButton = createModuleButton(
            this,
            tr("BIN 文件分析"),
            QStringLiteral("bin"),
            QStringLiteral("homeModuleButton"));
        auto *compareModuleButton = createModuleButton(
            this,
            tr("文件比较"),
            QStringLiteral("compare"),
            QStringLiteral("homeModuleButton"));
        moduleGrid->addWidget(serialModuleButton, 0, 0);
        moduleGrid->addWidget(convertModuleButton, 0, 1);
        moduleGrid->addWidget(binModuleButton, 0, 2);
        moduleGrid->addWidget(compareModuleButton, 0, 3);
        moduleGrid->setColumnStretch(0, 1);
        moduleGrid->setColumnStretch(1, 1);
        moduleGrid->setColumnStretch(2, 1);
        moduleGrid->setColumnStretch(3, 1);

        rootLayout->addLayout(moduleGrid);
        rootLayout->addStretch(1);

        connect(serialModuleButton, &QToolButton::clicked,
                this, &HomePage::openSerialAssistantRequested);
        connect(convertModuleButton, &QToolButton::clicked,
                this, &HomePage::openDataConvertRequested);
        connect(binModuleButton, &QToolButton::clicked,
                this, &HomePage::openBinAnalyzerRequested);
        connect(compareModuleButton, &QToolButton::clicked,
                this, &HomePage::openFileCompareRequested);
    }

    void HomePage::setSerialStatus(const QString &text, bool connected)
    {
        Q_UNUSED(text)
        Q_UNUSED(connected)
    }

    void HomePage::setCurrentFileStatus(const QString &text)
    {
        Q_UNUSED(text)
    }

    void HomePage::setTransferStats(qint64 txBytes, qint64 rxBytes)
    {
        Q_UNUSED(txBytes)
        Q_UNUSED(rxBytes)
    }

    void HomePage::appendSystemLog(const QString &text)
    {
        Q_UNUSED(text)
    }

    void HomePage::refreshRecentRecords()
    {
    }

} // namespace est
