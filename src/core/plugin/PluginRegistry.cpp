#include "plugin/PluginRegistry.h"

#include <QDebug>

namespace est {

PluginRegistry::PluginRegistry(QObject* parent)
    : QObject(parent)
{
}

PluginRegistry::~PluginRegistry()
{
    // 关闭所有插件（逆序）
    auto names = pluginNames();
    for (auto it = names.rbegin(); it != names.rend(); ++it) {
        IPlugin* p = m_plugins.take(*it);
        if (p) {
            p->shutdown();
        }
    }
}

bool PluginRegistry::registerPlugin(IPlugin* plugin)
{
    if (!plugin) return false;
    if (m_plugins.contains(plugin->name())) {
        qWarning() << "Plugin already registered:" << plugin->name();
        return false;
    }

    m_plugins.insert(plugin->name(), plugin);
    emit pluginRegistered(plugin->name());
    return true;
}

void PluginRegistry::unregisterPlugin(const QString& name)
{
    IPlugin* p = m_plugins.take(name);
    if (p) {
        p->shutdown();
        emit pluginUnregistered(name);
    }
}

IPlugin* PluginRegistry::plugin(const QString& name) const
{
    return m_plugins.value(name, nullptr);
}

QVector<IPlugin*> PluginRegistry::allPlugins() const
{
    return m_plugins.values();
}

QStringList PluginRegistry::pluginNames() const
{
    return m_plugins.keys();
}

bool PluginRegistry::hasPlugin(const QString& name) const
{
    return m_plugins.contains(name);
}

int PluginRegistry::count() const
{
    return m_plugins.size();
}

} // namespace est
