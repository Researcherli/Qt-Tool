#include "pages/SettingsPage.h"

#include <QApplication>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

namespace est
{

    SettingsPage::SettingsPage(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("settingsPage"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(30, 28, 30, 28);
        rootLayout->setSpacing(18);

        auto *titleLabel = new QLabel(tr("系统设置"), this);
        titleLabel->setObjectName(QStringLiteral("settingsTitle"));

        auto *subtitleLabel = new QLabel(
            tr("当前版本聚焦工业风界面、串口工具和可扩展调试框架。"),
            this);
        subtitleLabel->setObjectName(QStringLiteral("settingsSubtitle"));
        subtitleLabel->setWordWrap(true);

        auto *infoPanel = new QFrame(this);
        infoPanel->setObjectName(QStringLiteral("settingsPanel"));

        auto *formLayout = new QFormLayout(infoPanel);
        formLayout->setContentsMargins(18, 18, 18, 18);
        formLayout->setSpacing(12);
        formLayout->addRow(tr("应用名称："),
                           new QLabel(QApplication::applicationDisplayName(), infoPanel));
        formLayout->addRow(tr("应用版本："),
                           new QLabel(QApplication::applicationVersion(), infoPanel));
        formLayout->addRow(tr("主题风格："),
                           new QLabel(tr("工业深色主题（默认）"), infoPanel));
        formLayout->addRow(tr("配置存储："),
                           new QLabel(tr("使用 Qt 应用设置持久化串口参数"), infoPanel));

        rootLayout->addWidget(titleLabel);
        rootLayout->addWidget(subtitleLabel);
        rootLayout->addWidget(infoPanel);
        rootLayout->addStretch(1);
    }

} // namespace est
