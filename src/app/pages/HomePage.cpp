#include "pages/HomePage.h"

#include "plugin/ICore.h"
#include "widgets/ModuleIconFactory.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpacerItem>
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
            button->setIcon(QIcon(makeModuleIconPixmap(iconType, 128)));
            button->setIconSize(QSize(128, 128));
            button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            button->setMinimumHeight(200);
            button->setCursor(Qt::PointingHandCursor);
            return button;
        }

        QFrame *createStatusCard(QWidget *parent, const QString &label, const QString &initialValue,
                                  const QString &objectName)
        {
            auto *card = new QFrame(parent);
            card->setObjectName(objectName);
            card->setMinimumHeight(64);
            auto *layout = new QVBoxLayout(card);
            layout->setContentsMargins(12, 8, 12, 8);
            layout->setSpacing(2);

            auto *titleLabel = new QLabel(label, card);
            titleLabel->setObjectName(QStringLiteral("statusCardTitle"));
            titleLabel->setStyleSheet(QStringLiteral("font-size: 11px; color: #888;"));

            auto *valueLabel = new QLabel(initialValue, card);
            valueLabel->setObjectName(QStringLiteral("statusCardValue"));
            valueLabel->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold; color: #2F7FB5;"));
            valueLabel->setWordWrap(true);

            layout->addWidget(titleLabel);
            layout->addWidget(valueLabel);
            return card;
        }
    }

    HomePage::HomePage(ICore * /*core*/, QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("homePage"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(24, 20, 28, 24);
        rootLayout->setSpacing(16);

        // ---- 头部区域 ----
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

        // ---- 模块网格 ----
        auto *moduleGrid = new QGridLayout();
        moduleGrid->setContentsMargins(0, 0, 0, 0);
        moduleGrid->setHorizontalSpacing(16);
        moduleGrid->setVerticalSpacing(16);

        auto *serialModuleButton = createModuleButton(
            this, tr("串口工具"), QStringLiteral("serial"), QStringLiteral("homeModuleButton"));
        auto *terminalModuleButton = createModuleButton(
            this, tr("终端控制台"), QStringLiteral("terminal"), QStringLiteral("homeModuleButton"));
        auto *convertModuleButton = createModuleButton(
            this, tr("数据转换"), QStringLiteral("convert"), QStringLiteral("homeModuleButton"));
        auto *protocolModuleButton = createModuleButton(
            this, tr("协议解析"), QStringLiteral("protocol"), QStringLiteral("homeModuleButton"));
        auto *waveformModuleButton = createModuleButton(
            this, tr("波形图"), QStringLiteral("waveform"), QStringLiteral("homeModuleButton"));
        auto *rtosModuleButton = createModuleButton(
            this, tr("RTOS 监控"), QStringLiteral("rtos"), QStringLiteral("homeModuleButton"));
        auto *binModuleButton = createModuleButton(
            this, tr("BIN 文件分析"), QStringLiteral("bin"), QStringLiteral("homeModuleButton"));
        auto *compareModuleButton = createModuleButton(
            this, tr("文件比较"), QStringLiteral("compare"), QStringLiteral("homeModuleButton"));
        auto *guideModuleButton = createModuleButton(
            this, tr("使用手册"), QStringLiteral("guide"), QStringLiteral("homeModuleButton"));
        auto *canModuleButton = createModuleButton(
            this, tr("CAN 总线"), QStringLiteral("can"), QStringLiteral("homeModuleButton"));

        moduleGrid->addWidget(serialModuleButton, 0, 0);
        moduleGrid->addWidget(terminalModuleButton, 0, 1);
        moduleGrid->addWidget(convertModuleButton, 0, 2);
        moduleGrid->addWidget(protocolModuleButton, 0, 3);
        moduleGrid->addWidget(waveformModuleButton, 1, 0);
        moduleGrid->addWidget(rtosModuleButton, 1, 1);
        moduleGrid->addWidget(binModuleButton, 1, 2);
        moduleGrid->addWidget(compareModuleButton, 1, 3);
        moduleGrid->addWidget(canModuleButton, 2, 0);
        moduleGrid->addWidget(guideModuleButton, 2, 1, 1, 3);
        moduleGrid->setColumnStretch(0, 1);
        moduleGrid->setColumnStretch(1, 1);
        moduleGrid->setColumnStretch(2, 1);
        moduleGrid->setColumnStretch(3, 1);

        rootLayout->addLayout(moduleGrid);

        // ---- 按钮信号连接 ----
        connect(serialModuleButton, &QToolButton::clicked,
                this, &HomePage::openSerialAssistantRequested);
        connect(convertModuleButton, &QToolButton::clicked,
                this, &HomePage::openDataConvertRequested);
        connect(binModuleButton, &QToolButton::clicked,
                this, &HomePage::openBinAnalyzerRequested);
        connect(compareModuleButton, &QToolButton::clicked,
                this, &HomePage::openFileCompareRequested);
        connect(waveformModuleButton, &QToolButton::clicked,
                this, &HomePage::openWaveformRequested);
        connect(rtosModuleButton, &QToolButton::clicked,
                this, &HomePage::openRtosMonitorRequested);
        connect(guideModuleButton, &QToolButton::clicked,
                this, &HomePage::openUserGuideRequested);
        connect(protocolModuleButton, &QToolButton::clicked,
                this, &HomePage::openProtocolDecoderRequested);
        connect(terminalModuleButton, &QToolButton::clicked,
                this, &HomePage::openSerialConsoleRequested);
        connect(canModuleButton, &QToolButton::clicked,
                this, &HomePage::openCANBusRequested);
    }

    // ---------------------------------------------------------------------------
    // 状态更新方法（已弃用）
    // ---------------------------------------------------------------------------

    void HomePage::setSerialStatus(const QString & /*text*/, bool /*connected*/)
    {
    }

    void HomePage::setCurrentFileStatus(const QString & /*text*/)
    {
    }

    void HomePage::setTransferStats(qint64 /*txBytes*/, qint64 /*rxBytes*/)
    {
    }

    void HomePage::appendSystemLog(const QString & /*text*/)
    {
    }

    void HomePage::refreshRecentRecords()
    {
    }

} // namespace est
