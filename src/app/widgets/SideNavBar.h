#ifndef EST_SIDENAVBAR_H
#define EST_SIDENAVBAR_H

#include <QHash>
#include <QIcon>
#include <QList>
#include <QWidget>

class QLabel;
class QPropertyAnimation;
class QTimer;
class QVBoxLayout;

namespace est
{

    class NavIconButton;

    struct NavItem
    {
        QString id;
        QString text;
        QString iconName;
    };

    class SideNavBar : public QWidget
    {
        Q_OBJECT
        Q_PROPERTY(int sidebarWidth READ sidebarWidth WRITE setSidebarWidth NOTIFY sidebarWidthChanged)

    public:
        explicit SideNavBar(QWidget *parent = nullptr);

        void addNavigationItem(const QString &id, const QString &text, const QIcon &icon = QIcon());
        void addBottomNavigationItem(const QString &id, const QString &text, const QIcon &icon = QIcon());
        void setNavigationItemIcon(const QString &id, const QIcon &icon);
        void setNavigationItemText(const QString &id, const QString &text);
        void setCurrentId(const QString &id);
        void setPinnedExpanded(bool pinnedExpanded);

        int sidebarWidth() const;
        void setSidebarWidth(int width);

    protected:
        void enterEvent(QEnterEvent *event) override;
        void leaveEvent(QEvent *event) override;

    signals:
        void navigationRequested(const QString &id);
        void sidebarWidthChanged(int width);

    private slots:
        void onCollapseTimer();

    private:
        void setExpanded(bool expanded);

        QLabel *m_logoLabel = nullptr;
        QLabel *m_titleLabel = nullptr;
        QLabel *m_versionLabel = nullptr;
        QVBoxLayout *m_navLayout = nullptr;
        QVBoxLayout *m_bottomNavLayout = nullptr;
        bool m_expanded = true; // 初始 true，确保构造函数 setExpanded(false) 生效
        bool m_pinnedExpanded = false;

        int m_collapsedWidth = 72;
        int m_expandedWidth = 200;

        QPropertyAnimation *m_expandAnimation = nullptr;
        QTimer *m_collapseTimer = nullptr;

        QList<NavIconButton *> m_buttons;
        QHash<QString, NavIconButton *> m_buttonById;
        NavIconButton *m_currentButton = nullptr;
    };

    /// 单个导航按钮：图标 + 文字（展开时显示）
    class NavIconButton : public QWidget
    {
        Q_OBJECT

    public:
        explicit NavIconButton(const NavItem &item, QWidget *parent = nullptr);

        void setExpanded(bool expanded);
        void setSelected(bool selected);
        void setIcon(const QIcon &icon);
        void setText(const QString &text);

        QString id() const { return m_item.id; }

    signals:
        void clicked(const QString &id);

    protected:
        void paintEvent(QPaintEvent *event) override;
        void enterEvent(QEnterEvent *event) override;
        void leaveEvent(QEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;

    private:
        NavItem m_item;
        QIcon m_icon;
        bool m_expanded = true; // 初始 true，确保 setExpanded(false) 生效
        bool m_selected = false;
        bool m_hovered = false;
        bool m_pressed = false;
    };

} // namespace est

#endif // EST_SIDENAVBAR_H
