#ifndef EST_IVIEWFACTORY_H
#define EST_IVIEWFACTORY_H

#include <Qt>
#include <QString>

class QWidget;

namespace est {

/**
 * 视图工厂 — 插件通过此接口注册 UI 面板。
 *
 * AppShell 负责调用 createWidget() 创建面板，并将其作为 QDockWidget 添加到主窗口。
 */
class IViewFactory {
public:
    virtual ~IViewFactory() = default;

    /// 视图唯一标识（如 "can-trace-view"）
    virtual QString id() const = 0;

    /// 用户可见的显示名称（如 "CAN 报文监视"）
    virtual QString displayName() const = 0;

    /// 默认停靠区域
    virtual Qt::DockWidgetArea defaultArea() const = 0;

    /// 创建视图组件。AppShell 负责销毁。
    virtual QWidget* createWidget(QWidget* parent) = 0;
};

} // namespace est

#endif // EST_IVIEWFACTORY_H
