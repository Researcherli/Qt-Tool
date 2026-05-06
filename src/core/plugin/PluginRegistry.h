#ifndef EST_PLUGINREGISTRY_H
#define EST_PLUGINREGISTRY_H

#include "plugin/IPlugin.h"

#include <QHash>
#include <QObject>
#include <QString>

namespace est {

/**
 * 插件注册表 — 管理所有已加载插件的生命周期和查询。
 */
class PluginRegistry : public QObject {
    Q_OBJECT

public:
    explicit PluginRegistry(QObject* parent = nullptr);
    ~PluginRegistry() override;

    /// 注册一个插件实例（由 PluginLoader 调用）
    bool registerPlugin(IPlugin* plugin);

    /// 注销插件
    void unregisterPlugin(const QString& name);

    /// 按名称查找插件
    IPlugin* plugin(const QString& name) const;

    /// 获取所有已注册插件
    QVector<IPlugin*> allPlugins() const;

    /// 获取所有插件名称
    QStringList pluginNames() const;

    /// 检查插件是否已注册
    bool hasPlugin(const QString& name) const;

    /// 已注册插件数量
    int count() const;

signals:
    void pluginRegistered(const QString& name);
    void pluginUnregistered(const QString& name);

private:
    QHash<QString, IPlugin*> m_plugins;
};

} // namespace est

#endif // EST_PLUGINREGISTRY_H
