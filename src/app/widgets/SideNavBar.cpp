#include "widgets/SideNavBar.h"

#include <QEnterEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QCursor>
#include <QPropertyAnimation>
#include <QPixmap>
#include <QTimer>
#include <QVBoxLayout>

namespace est
{
    namespace
    {
        QPixmap makeAppLogoPixmap(int size)
        {
            QPixmap pixmap(size, size);
            pixmap.fill(Qt::transparent);

            QPainter p(&pixmap);
            p.setRenderHint(QPainter::Antialiasing);

            const QRectF board(size * 0.12, size * 0.16, size * 0.76, size * 0.68);
            QPainterPath boardPath;
            boardPath.addRoundedRect(board, size * 0.09, size * 0.09);
            p.fillPath(boardPath, QColor(QStringLiteral("#DCECF8")));

            p.setPen(QPen(QColor(QStringLiteral("#173E63")), qMax(2, size / 24)));
            p.drawPath(boardPath);

            p.setPen(QPen(QColor(QStringLiteral("#2F7FB5")), qMax(2, size / 24), Qt::SolidLine, Qt::RoundCap));
            const qreal pinIn = size * 0.12;
            const qreal pinOut = size * 0.04;
            const qreal pinRightIn = size * 0.88;
            const qreal pinRightOut = size * 0.96;
            for (int i = 0; i < 4; ++i)
            {
                const qreal y = size * (0.28 + i * 0.14);
                p.drawLine(QPointF(pinOut, y), QPointF(pinIn, y));
                p.drawLine(QPointF(pinRightIn, y), QPointF(pinRightOut, y));
            }

            const qreal pinTop = size * 0.16;
            const qreal pinTopOut = size * 0.08;
            const qreal pinBottom = size * 0.84;
            const qreal pinBottomOut = size * 0.92;
            for (int i = 0; i < 3; ++i)
            {
                const qreal x = size * (0.32 + i * 0.18);
                p.drawLine(QPointF(x, pinTopOut), QPointF(x, pinTop));
                p.drawLine(QPointF(x, pinBottom), QPointF(x, pinBottomOut));
            }

            const QRectF chip(size * 0.26, size * 0.3, size * 0.48, size * 0.4);
            QPainterPath chipPath;
            chipPath.addRoundedRect(chip, size * 0.045, size * 0.045);
            p.fillPath(chipPath, QColor(QStringLiteral("#2F7FB5")));
            p.setPen(QPen(QColor(QStringLiteral("#173E63")), qMax(2, size / 28)));
            p.drawPath(chipPath);

            p.setPen(QPen(QColor(QStringLiteral("#FFFFFF")), qMax(2, size / 28), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPainterPath signalPath;
            signalPath.moveTo(size * 0.34, size * 0.53);
            signalPath.lineTo(size * 0.42, size * 0.53);
            signalPath.lineTo(size * 0.46, size * 0.43);
            signalPath.lineTo(size * 0.54, size * 0.62);
            signalPath.lineTo(size * 0.59, size * 0.49);
            signalPath.lineTo(size * 0.68, size * 0.49);
            p.drawPath(signalPath);

            p.setBrush(QColor(QStringLiteral("#FFFFFF")));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(size * 0.34, size * 0.41), size * 0.025, size * 0.025);
            p.drawEllipse(QPointF(size * 0.66, size * 0.61), size * 0.025, size * 0.025);

            return pixmap;
        }
    }

    // ---------------------------------------------------------------------------
    //  NavIconButton — 单个导航按钮（图标 + 可选文字）
    // ---------------------------------------------------------------------------

    NavIconButton::NavIconButton(const NavItem &item, QWidget *parent)
        : QWidget(parent), m_item(item)
    {
        setObjectName(QStringLiteral("navIconButton"));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setFixedHeight(48);
        setCursor(Qt::PointingHandCursor);
        setToolTip(item.text);
    }

    void NavIconButton::setExpanded(bool expanded)
    {
        if (m_expanded == expanded)
            return;
        m_expanded = expanded;
        setToolTip(expanded ? QString() : m_item.text);
        update();
    }

    void NavIconButton::setSelected(bool selected)
    {
        if (m_selected == selected)
            return;
        m_selected = selected;
        update();
    }

    void NavIconButton::setIcon(const QIcon &icon)
    {
        m_icon = icon;
        update();
    }

    void NavIconButton::setText(const QString &text)
    {
        if (m_item.text == text)
            return;

        m_item.text = text;
        setToolTip(m_expanded ? QString() : m_item.text);
        update();
    }

    void NavIconButton::paintEvent(QPaintEvent * /*event*/)
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int w = width();
        const int h = height();
        const int radius = 6;
        const int margin = 4;

        // 背景
        if (m_selected)
        {
            QPainterPath bg;
            bg.addRoundedRect(margin, margin, w - 2 * margin, h - 2 * margin, radius, radius);
            p.fillPath(bg, QColor(QStringLiteral("#2F7FB5")));
        }
        else if (m_hovered)
        {
            QPainterPath bg;
            bg.addRoundedRect(margin, margin, w - 2 * margin, h - 2 * margin, radius, radius);
            p.fillPath(bg, QColor(QStringLiteral("#FFFFFF")));
        }

        // 图标区域
        const int iconSize = m_expanded ? 26 : 34;
        const int iconX = m_expanded ? 14 : (w - iconSize) / 2;
        const int iconY = (h - iconSize) / 2;

        if (!m_icon.isNull())
        {
            // 绘制真实图标
            QRect iconRect(iconX, iconY, iconSize, iconSize);
            QPixmap pixmap = m_icon.pixmap(iconSize, iconSize,
                                           m_selected ? QIcon::Selected : QIcon::Normal);
            p.drawPixmap(iconRect, pixmap);
        }
        else
        {
            // 用首字母作为图标绘制
            const int circleRadius = m_expanded ? 14 : 18;
            const int circleCenterX = iconX + iconSize / 2;
            const int circleCenterY = iconY + iconSize / 2;

            if (!m_selected)
            {
                QPainterPath circle;
                circle.addEllipse(circleCenterX - circleRadius, circleCenterY - circleRadius,
                                  circleRadius * 2, circleRadius * 2);
                p.fillPath(circle, QColor(QStringLiteral("#DCECF8")));
            }
            else
            {
                QPainterPath circle;
                circle.addEllipse(circleCenterX - circleRadius, circleCenterY - circleRadius,
                                  circleRadius * 2, circleRadius * 2);
                p.fillPath(circle, QColor(QStringLiteral("#256E9F")));
            }

            QFont iconFont(QStringLiteral("Segoe UI"), m_expanded ? 11 : 14, QFont::Bold);
            p.setFont(iconFont);
            p.setPen(m_selected ? Qt::white : QColor(QStringLiteral("#173E63")));

            // Use first ASCII character or abbreviation
            const QString letter = m_item.text.left(1);
            // If it's a CJK character (unicode > 0x4E00), use a generic symbol instead
            QString displayChar = letter;
            if (!letter.isEmpty() && letter.at(0).unicode() >= 0x4E00) {
                // Use first ASCII character from the nav id as abbreviation
                QChar firstAscii;
                for (const QChar &ch : m_item.text) {
                    if (ch.unicode() < 0x4E00 && ch.isLetter()) {
                        firstAscii = ch;
                        break;
                    }
                }
                displayChar = firstAscii.isNull() ? QStringLiteral("☰") : QString(firstAscii);
            }
            p.drawText(QRect(circleCenterX - circleRadius, circleCenterY - circleRadius,
                             circleRadius * 2, circleRadius * 2),
                       Qt::AlignCenter, displayChar);
        }

        // 文字 — 仅展开时显示
        if (m_expanded)
        {
            QFont textFont(QStringLiteral("Segoe UI"), 12);
            p.setFont(textFont);

            p.setPen(m_selected ? Qt::white : QColor(QStringLiteral("#203447")));

            const int textX = iconX + iconSize + 10;
            const int textW = w - textX - 10;
            if (textW > 0)
            {
                p.drawText(QRect(textX, 0, textW, h), Qt::AlignVCenter | Qt::AlignLeft, m_item.text);
            }
        }
    }

    void NavIconButton::enterEvent(QEnterEvent *event)
    {
        QWidget::enterEvent(event);
        m_hovered = true;
        update();
    }

    void NavIconButton::leaveEvent(QEvent *event)
    {
        QWidget::leaveEvent(event);
        m_hovered = false;
        update();
    }

    void NavIconButton::mousePressEvent(QMouseEvent *event)
    {
        QWidget::mousePressEvent(event);
        m_pressed = true;
        update();
    }

    void NavIconButton::mouseReleaseEvent(QMouseEvent *event)
    {
        QWidget::mouseReleaseEvent(event);
        m_pressed = false;
        update();
        emit clicked(m_item.id);
    }

    // ---------------------------------------------------------------------------
    //  SideNavBar — 侧边导航栏
    // ---------------------------------------------------------------------------

    SideNavBar::SideNavBar(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("sideNavBar"));
        setMouseTracking(true);

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 16, 0, 16);
        rootLayout->setSpacing(0);
        rootLayout->setAlignment(Qt::AlignTop);

        auto *brandHeader = new QWidget(this);
        brandHeader->setObjectName(QStringLiteral("sideNavBrandHeader"));
        brandHeader->setFixedHeight(66);
        auto *brandLayout = new QHBoxLayout(brandHeader);
        brandLayout->setContentsMargins(0, 0, 0, 0);
        brandLayout->setSpacing(8);
        brandLayout->setAlignment(Qt::AlignCenter);

        // 顶部大图标区域 — 收缩时仅显示大图标，展开时显示大图标 + 名称
        m_logoLabel = new QLabel(brandHeader);
        m_logoLabel->setObjectName(QStringLiteral("sideNavLogo"));
        m_logoLabel->setAlignment(Qt::AlignCenter);
        m_logoLabel->setFixedSize(48, 48);
        m_logoLabel->setPixmap(makeAppLogoPixmap(44));

        m_titleLabel = new QLabel(brandHeader);
        m_titleLabel->setObjectName(QStringLiteral("sideNavTitle"));
        m_titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        m_titleLabel->setText(tr("EST Studio"));

        brandLayout->addWidget(m_logoLabel);
        brandLayout->addWidget(m_titleLabel);

        // 版本号
        m_versionLabel = new QLabel(tr("v2.0"), this);
        m_versionLabel->setObjectName(QStringLiteral("sideNavVersion"));
        m_versionLabel->setAlignment(Qt::AlignCenter);
        m_versionLabel->setFixedHeight(20);

        // 导航按钮容器 — 关键：用 QWidget 包装，避免 QLayout 在宽度变化时重新分配空间
        auto *navContainer = new QWidget(this);
        navContainer->setObjectName(QStringLiteral("navContainer"));
        navContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_navLayout = new QVBoxLayout(navContainer);
        m_navLayout->setContentsMargins(6, 8, 6, 8);
        m_navLayout->setSpacing(4);
        m_navLayout->setAlignment(Qt::AlignTop);

        auto *bottomNavContainer = new QWidget(this);
        bottomNavContainer->setObjectName(QStringLiteral("bottomNavContainer"));
        bottomNavContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_bottomNavLayout = new QVBoxLayout(bottomNavContainer);
        m_bottomNavLayout->setContentsMargins(6, 8, 6, 4);
        m_bottomNavLayout->setSpacing(4);
        m_bottomNavLayout->setAlignment(Qt::AlignBottom);

        rootLayout->addWidget(brandHeader);
        rootLayout->addSpacing(8);
        rootLayout->addWidget(navContainer, 0, Qt::AlignTop);
        rootLayout->addStretch(1);
        rootLayout->addWidget(bottomNavContainer, 0, Qt::AlignBottom);
        rootLayout->addWidget(m_versionLabel, 0, Qt::AlignHCenter | Qt::AlignBottom);

        // 展开收缩动画
        m_expandAnimation = new QPropertyAnimation(this, "sidebarWidth");
        m_expandAnimation->setDuration(180);
        m_expandAnimation->setEasingCurve(QEasingCurve::InOutCubic);

        // 延迟收缩定时器
        m_collapseTimer = new QTimer(this);
        m_collapseTimer->setSingleShot(true);
        m_collapseTimer->setInterval(200);
        connect(m_collapseTimer, &QTimer::timeout, this, &SideNavBar::onCollapseTimer);

        // 直接设置初始折叠状态（跳过动画，避免启动时过渡闪烁）
        m_expanded = false;
        setFixedWidth(m_collapsedWidth);
        m_titleLabel->setVisible(false);
        m_versionLabel->setVisible(false);
    }

    int SideNavBar::sidebarWidth() const
    {
        return width();
    }

    void SideNavBar::setSidebarWidth(int w)
    {
        setFixedWidth(w);
    }

    void SideNavBar::addNavigationItem(const QString &id, const QString &text, const QIcon &icon)
    {
        NavItem item{id, text, QString()};

        auto *btn = new NavIconButton(item, this);
        if (!icon.isNull())
        {
            btn->setIcon(icon);
        }
        m_buttons.append(btn);
        m_buttonById.insert(id, btn);

        connect(btn, &NavIconButton::clicked, this, [this](const QString &clickedId)
                {
            setCurrentId(clickedId);
            emit navigationRequested(clickedId); });

        m_navLayout->addWidget(btn);

        btn->setExpanded(m_expanded);

        // 默认选中第一个
        if (m_buttons.size() == 1)
        {
            m_currentButton = btn;
            btn->setSelected(true);
        }
    }

    void SideNavBar::addBottomNavigationItem(const QString &id, const QString &text, const QIcon &icon)
    {
        if (m_bottomNavLayout == nullptr)
            return;

        NavItem item{id, text, QString()};

        auto *btn = new NavIconButton(item, this);
        if (!icon.isNull())
        {
            btn->setIcon(icon);
        }
        m_buttons.append(btn);
        m_buttonById.insert(id, btn);

        connect(btn, &NavIconButton::clicked, this, [this](const QString &clickedId)
                {
            setCurrentId(clickedId);
            emit navigationRequested(clickedId); });

        m_bottomNavLayout->addWidget(btn);
        btn->setExpanded(m_expanded);
    }

    void SideNavBar::setNavigationItemIcon(const QString &id, const QIcon &icon)
    {
        auto it = m_buttonById.find(id);
        if (it != m_buttonById.end())
            it.value()->setIcon(icon);
    }

    void SideNavBar::setNavigationItemText(const QString &id, const QString &text)
    {
        auto it = m_buttonById.find(id);
        if (it != m_buttonById.end())
            it.value()->setText(text);
    }

    void SideNavBar::setCurrentId(const QString &id)
    {
        auto it = m_buttonById.find(id);
        if (it == m_buttonById.end())
            return;

        if (m_currentButton && m_currentButton == it.value())
            return;

        if (m_currentButton)
            m_currentButton->setSelected(false);

        m_currentButton = it.value();
        m_currentButton->setSelected(true);
    }

    void SideNavBar::setPinnedExpanded(bool pinnedExpanded)
    {
        if (m_pinnedExpanded == pinnedExpanded)
            return;

        m_pinnedExpanded = pinnedExpanded;
        m_collapseTimer->stop();
        setExpanded(m_pinnedExpanded);
    }

    void SideNavBar::enterEvent(QEnterEvent *event)
    {
        QWidget::enterEvent(event);
        m_collapseTimer->stop();
    }

    void SideNavBar::leaveEvent(QEvent *event)
    {
        QWidget::leaveEvent(event);
        if (!m_pinnedExpanded)
        {
            m_collapseTimer->start();
        }
    }

    void SideNavBar::onCollapseTimer()
    {
        // 确认鼠标确实已经离开整个 SideNavBar 区域后才收缩
        if (!rect().contains(mapFromGlobal(QCursor::pos())))
        {
            setExpanded(m_pinnedExpanded);
        }
    }

    void SideNavBar::setExpanded(bool expanded)
    {
        if (m_expanded == expanded)
            return;

        m_expanded = expanded;

        // 停止上一个动画，防止快速 hover 时多个动画冲突
        m_expandAnimation->stop();
        m_expandAnimation->setStartValue(width());
        m_expandAnimation->setEndValue(expanded ? m_expandedWidth : m_collapsedWidth);
        m_expandAnimation->start();

        // 版本号仅在展开时显示
        m_versionLabel->setVisible(expanded);

        if (expanded)
        {
            m_titleLabel->setVisible(true);
        }
        else
        {
            m_titleLabel->setVisible(false);
        }

        // 所有按钮切换展开状态
        for (auto *btn : m_buttons)
        {
            btn->setExpanded(expanded);
        }

        update();
    }

} // namespace est
