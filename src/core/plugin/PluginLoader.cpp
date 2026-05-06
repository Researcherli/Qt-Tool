#include "plugin/PluginLoader.h"
#include "plugin/PluginRegistry.h"
#include "plugin/ICore.h"

#include <QDebug>
#include <QDir>
#include <QLibrary>
#include <QPluginLoader>
#include <QQueue>

namespace est {

PluginLoader::PluginLoader(ICore* core, PluginRegistry* registry,
                             QObject* parent)
    : QObject(parent)
    , m_core(core)
    , m_registry(registry)
{
}

PluginLoader::~PluginLoader()
{
    unloadAll();
}

void PluginLoader::addPluginPath(const QString& path)
{
    if (!m_pluginPaths.contains(path)) {
        m_pluginPaths.append(path);
    }
}

bool PluginLoader::loadAll()
{
    const QStringList pluginFiles = discoverPlugins();
    if (pluginFiles.isEmpty()) {
        qDebug() << "No plugins found in paths:" << m_pluginPaths;
        emit allLoaded();
        return true;
    }

    // 第一遍：加载所有插件文件，获取 IPlugin 实例
    QVector<IPlugin*> plugins;
    for (const QString& filePath : pluginFiles) {
        IPlugin* plugin = loadPluginFile(filePath);
        if (plugin) {
            plugins.append(plugin);
        }
    }

    // 第二遍：按依赖拓扑排序
    QVector<IPlugin*> sorted = topologicalSort(plugins);

    // 第三遍：按序初始化并注册
    for (IPlugin* plugin : sorted) {
        if (plugin->initialize(m_core)) {
            m_registry->registerPlugin(plugin);
            emit pluginLoaded(plugin->name());
            qDebug() << "Plugin loaded:" << plugin->name() << "v" << plugin->version();
        } else {
            qWarning() << "Plugin initialization failed:" << plugin->name();
            emit pluginLoadFailed(plugin->name(), "Initialization failed");
        }
    }

    emit allLoaded();
    return true;
}

void PluginLoader::unloadAll()
{
    // PluginRegistry 析构时会 shutdown + delete
    m_loaders.clear();
}

int PluginLoader::loadedCount() const
{
    return m_registry->count();
}

QStringList PluginLoader::discoverPlugins() const
{
    QStringList result;
    for (const QString& path : m_pluginPaths) {
        QDir dir(path);
        if (!dir.exists()) continue;

        const QStringList entries = dir.entryList(
            QDir::Files | QDir::NoSymLinks);

        for (const QString& entry : entries) {
            // Windows: .dll, Linux: .so, macOS: .dylib
            if (QLibrary::isLibrary(entry)) {
                result.append(dir.absoluteFilePath(entry));
            }
        }
    }
    return result;
}

IPlugin* PluginLoader::loadPluginFile(const QString& filePath)
{
    QPluginLoader* loader = new QPluginLoader(filePath, this);
    QObject* instance = loader->instance();

    if (!instance) {
        qWarning() << "Failed to load plugin:" << filePath
                    << "Error:" << loader->errorString();
        emit pluginLoadFailed(filePath, loader->errorString());
        delete loader;
        return nullptr;
    }

    IPlugin* plugin = qobject_cast<IPlugin*>(instance);
    if (!plugin) {
        qWarning() << "Plugin does not implement IPlugin:" << filePath;
        emit pluginLoadFailed(filePath, "Not an IPlugin");
        loader->unload();
        delete loader;
        return nullptr;
    }

    m_loaders.append(loader);
    return plugin;
}

QVector<IPlugin*> PluginLoader::topologicalSort(const QVector<IPlugin*>& plugins) const
{
    // Kahn's algorithm
    QHash<QString, IPlugin*> byName;
    QHash<QString, int> inDegree;
    QHash<QString, QStringList> dependents; // name → 谁依赖它

    for (IPlugin* p : plugins) {
        byName[p->name()] = p;
        inDegree[p->name()] = 0;
    }

    for (IPlugin* p : plugins) {
        for (const QString& dep : p->dependencies()) {
            if (byName.contains(dep)) {
                inDegree[p->name()]++;
                dependents[dep].append(p->name());
            } else {
                qWarning() << "Plugin" << p->name()
                            << "depends on missing plugin:" << dep;
            }
        }
    }

    QQueue<QString> queue;
    for (auto it = inDegree.begin(); it != inDegree.end(); ++it) {
        if (it.value() == 0) {
            queue.enqueue(it.key());
        }
    }

    QVector<IPlugin*> sorted;
    while (!queue.isEmpty()) {
        QString name = queue.dequeue();
        sorted.append(byName[name]);

        for (const QString& depName : dependents.value(name)) {
            int& deg = inDegree[depName];
            if (--deg == 0) {
                queue.enqueue(depName);
            }
        }
    }

    if (sorted.size() != plugins.size()) {
        qWarning() << "Circular dependency detected among plugins!";
        // 将未排序的插件追加到末尾（尽力而为）
        for (IPlugin* p : plugins) {
            if (!sorted.contains(p)) {
                sorted.append(p);
            }
        }
    }

    return sorted;
}

} // namespace est
