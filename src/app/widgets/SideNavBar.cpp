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
#include <QtMath>

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

            const QRectF body(size * 0.18, size * 0.18, size * 0.64, size * 0.64);
            QPainterPath bodyPath;
            bodyPath.addRoundedRect(body, size * 0.12, size * 0.12);
            p.fillPath(bodyPath, QColor(QStringLiteral("#2F7FB5")));
            p.setPen(QPen(QColor(QStringLiteral("#173E63")), qMax(1, size / 18)));
            p.drawPath(bodyPath);

            p.setPen(QPen(QColor(QStringLiteral("#DCECF8")), qMax(1, size / 18), Qt::SolidLine, Qt::RoundCap));
            const int pinStart = qRound(size * 0.08);
            const int pinEnd = qRound(size * 0.18);
            const int pinOutStart = qRound(size * 0.82);
            const int pinOutEnd = qRound(size * 0.92);
            for (int i = 0; i < 3; ++i)
            {
                const int pos = qRound(size * (0.32 + i * 0.18));
                p.drawLine(pinStart, pos, pinEnd, pos);
                p.drawLine(pinOutStart, pos, pinOutEnd, pos);
                p.drawLine(pos, pinStart, pos, pinEnd);
                p.drawLine(pos, pinOutStart, pos, pinOutEnd);
            }

            p.setPen(QPen(QColor(QStringLiteral("#FFFFFF")), qMax(2, size / 12), Qt::SolidLine, Qt::RoundCap));
            p.drawLine(QPointF(size * 0.34, size * 0.52), QPointF(size * 0.48, size * 0.64));
            p.drawLine(QPointF(size * 0.48, size * 0.64), QPointF(size * 0.68, size * 0.38));

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
    }

    void NavIconButton::setExpanded(bool expanded)
    {
        if (m_expanded == expanded)
            return;
        m_expanded = expanded;
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

            const QString letter = m_item.text.left(1);
            p.drawText(QRect(circleCenterX - circleRadius, circleCenterY - circleRadius,
                             circleRadius * 2, circleRadius * 2),
                       Qt::AlignCenter, letter);
        }

        // 文字 — 仅展开时显示
        if (m_expanded)
        {
            QFont textFont(QStringLiteral("Segoe UI"), 12);
            if (m_selected)
                textFont.setWeight(QFont::DemiBold);
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
        m_titleLabel->setText(tr("嵌入式工具"));

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

        rootLayout->addWidget(brandHeader);
        rootLayout->addSpacing(8);
        rootLayout->addWidget(navContainer, 0, Qt::AlignTop);
        rootLayout->addStretch(1);
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

        setExpanded(false);
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

        // 默认选中第一个
        if (m_buttons.size() == 1)
        {
            m_currentButton = btn;
            btn->setSelected(true);
        }

        btn->setExpanded(m_expanded);
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

    void SideNavBar::enterEvent(QEnterEvent *event)
    {
        QWidget::enterEvent(event);
        m_collapseTimer->stop();
        setExpanded(true);
    }

    void SideNavBar::leaveEvent(QEvent *event)
    {
        QWidget::leaveEvent(event);
        m_collapseTimer->start();
    }

    void SideNavBar::onCollapseTimer()
    {
        // 确认鼠标确实已经离开整个 SideNavBar 区域后才收缩
        if (!rect().contains(mapFromGlobal(QCursor::pos())))
        {
            setExpanded(false);
        }
    }

    void SideNavBar::setExpanded(bool expanded)
    {
        if (m_expanded == expanded)
            return;

        m_expanded = expanded;

        // 动画切换宽度
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
