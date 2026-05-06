#include "pages/HomePage.h"

#include "plugin/ICore.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QVBoxLayout>

namespace est
{
    namespace
    {
        QPixmap makeModuleIconPixmap(const QString &type, int size)
        {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);

            QPainter p(&pixmap);
            p.setRenderHint(QPainter::Antialiasing);

            if (type == QStringLiteral("serial"))
            {
                const QRectF screen(size * 0.16, size * 0.22, size * 0.68, size * 0.44);
                QPainterPath monitor;
                monitor.addRoundedRect(screen, size * 0.05, size * 0.05);
                p.fillPath(monitor, QColor(QStringLiteral("#2F7FB5")));
                p.fillRect(screen.adjusted(size * 0.08, size * 0.08, -size * 0.08, -size * 0.1),
                           QColor(QStringLiteral("#EAF2FA")));
                p.setPen(QPen(QColor(QStringLiteral("#173E63")), qMax(2, size / 26)));
                p.drawPath(monitor);
                p.setBrush(QColor(QStringLiteral("#2F7FB5")));
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(QRectF(size * 0.42, size * 0.66, size * 0.16, size * 0.1), size * 0.02, size * 0.02);
                p.drawRoundedRect(QRectF(size * 0.32, size * 0.76, size * 0.36, size * 0.06), size * 0.02, size * 0.02);
            }
            else if (type == QStringLiteral("convert"))
            {
                const QRectF doc(size * 0.26, size * 0.14, size * 0.48, size * 0.68);
                QPainterPath docPath;
                docPath.addRoundedRect(doc, size * 0.04, size * 0.04);
                p.fillPath(docPath, QColor(QStringLiteral("#FFFFFF")));
                p.setPen(QPen(QColor(QStringLiteral("#173E63")), qMax(2, size / 28)));
                p.drawPath(docPath);

                p.setPen(QPen(QColor(QStringLiteral("#2F7FB5")), qMax(2, size / 24), Qt::SolidLine, Qt::RoundCap));
                p.drawLine(QPointF(size * 0.36, size * 0.34), QPointF(size * 0.62, size * 0.34));
                p.drawLine(QPointF(size * 0.36, size * 0.48), QPointF(size * 0.62, size * 0.48));
                p.drawLine(QPointF(size * 0.36, size * 0.62), QPointF(size * 0.56, size * 0.62));
            }
            else
            {
                const QRectF file(size * 0.28, size * 0.14, size * 0.44, size * 0.68);
                QPainterPath filePath;
                filePath.addRoundedRect(file, size * 0.035, size * 0.035);
                p.fillPath(filePath, QColor(QStringLiteral("#FFFFFF")));
                p.setPen(QPen(QColor(QStringLiteral("#173E63")), qMax(2, size / 26)));
                p.drawPath(filePath);

                p.setBrush(QColor(QStringLiteral("#DCECF8")));
                p.setPen(Qt::NoPen);
                for (int i = 0; i < 3; ++i)
                {
                    p.drawRoundedRect(QRectF(size * 0.38, size * (0.34 + i * 0.13), size * 0.24, size * 0.055),
                                      size * 0.01, size * 0.01);
                }
                p.setPen(QPen(QColor(QStringLiteral("#2F7FB5")), qMax(2, size / 24), Qt::SolidLine, Qt::RoundCap));
                p.drawLine(QPointF(size * 0.34, size * 0.78), QPointF(size * 0.66, size * 0.78));
            }

            return pixmap;
        }

        QPixmap makeHomeIconPixmap(int size)
        {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);

            QPainter p(&pixmap);
            p.setRenderHint(QPainter::Antialiasing);

            const QRectF house(size * 0.2, size * 0.38, size * 0.6, size * 0.42);
            QPainterPath body;
            body.addRoundedRect(house, size * 0.06, size * 0.06);
            p.fillPath(body, QColor(QStringLiteral("#2F7FB5")));

            QPainterPath roof;
            roof.moveTo(size * 0.16, size * 0.42);
            roof.lineTo(size * 0.5, size * 0.14);
            roof.lineTo(size * 0.84, size * 0.42);
            roof.closeSubpath();
            p.fillPath(roof, QColor(QStringLiteral("#173E63")));

            p.setBrush(QColor(QStringLiteral("#EEF5FB")));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(QRectF(size * 0.43, size * 0.56, size * 0.14, size * 0.24),
                              size * 0.02, size * 0.02);

            return pixmap;
        }

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

        moduleGrid->addWidget(serialModuleButton, 0, 0);
        moduleGrid->addWidget(convertModuleButton, 0, 1);
        moduleGrid->addWidget(binModuleButton, 0, 2);
        moduleGrid->setColumnStretch(0, 1);
        moduleGrid->setColumnStretch(1, 1);
        moduleGrid->setColumnStretch(2, 1);

        rootLayout->addLayout(moduleGrid);
        rootLayout->addStretch(1);

        connect(serialModuleButton, &QToolButton::clicked,
                this, &HomePage::openSerialAssistantRequested);
        connect(convertModuleButton, &QToolButton::clicked,
                this, &HomePage::openDataConvertRequested);
        connect(binModuleButton, &QToolButton::clicked,
                this, &HomePage::openBinAnalyzerRequested);
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
