#ifndef EST_ICORE_H
#define EST_ICORE_H

namespace est {

class DataBus;
class PluginRegistry;
class TransportRegistry;
class RecentRecordManager;

/**
 * 核心服务访问接口 — 插件通过 ICore 访问平台基础设施。
 *
 * 由 AppShell 实现，在 IPlugin::initialize() 时注入。
 */
class ICore {
public:
    virtual ~ICore() = default;

    virtual DataBus* dataBus() const = 0;
    virtual PluginRegistry* pluginRegistry() const = 0;
    virtual TransportRegistry* transportRegistry() const = 0;
    virtual RecentRecordManager* recentRecordManager() const = 0;
};

} // namespace est

#endif // EST_ICORE_H
