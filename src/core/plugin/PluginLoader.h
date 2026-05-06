#ifndef EST_PLUGINLOADER_H
#define EST_PLUGINLOADER_H

#include "plugin/IPlugin.h"

#include <QObject>
#include <QString>

class QPluginLoader;

namespace est {

class ICore;
class PluginRegistry;

/**
 * 插件加载器 — 从指定目录发现并加载 Qt 共享库插件。
 *
 * 加载流程：
 *   1. 扫描插件目录，发现所有符合接口的共享库
 *   2. 解析依赖关系，拓扑排序
 *   3. 按序调用 IPlugin::initialize()
 *   4. 注册到 PluginRegistry
 */
class PluginLoader : public QObject {
    Q_OBJECT

public:
    explicit PluginLoader(ICore* core, PluginRegistry* registry,
                           QObject* parent = nullptr);
    ~PluginLoader() override;

    /// 添加插件搜索路径
    void addPluginPath(const QString& path);

    /// 扫描并加载所有插件（按依赖排序）
    bool loadAll();

    /// 卸载所有插件
    void unloadAll();

    /// 已加载插件数量
    int loadedCount() const;

signals:
    void pluginLoaded(const QString& name);
    void pluginLoadFailed(const QString& filePath, const QString& error);
    void allLoaded();

private:
    /// 发现目录中的所有插件
    QStringList discoverPlugins() const;

    /// 对插件列表按依赖拓扑排序
    QVector<IPlugin*> topologicalSort(const QVector<IPlugin*>& plugins) const;

    /// 加载单个插件文件
    IPlugin* loadPluginFile(const QString& filePath);

    ICore* m_core;
    PluginRegistry* m_registry;
    QStringList m_pluginPaths;
    QVector<QPluginLoader*> m_loaders;
};

} // namespace est

#endif // EST_PLUGINLOADER_H
