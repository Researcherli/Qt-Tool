#ifndef EST_IPLUGIN_H
#define EST_IPLUGIN_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace est {

class ICore;
class IViewFactory;

class IPlugin {
public:
    virtual ~IPlugin() = default;

    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual QString displayName() const = 0;
    virtual QStringList dependencies() const { return {}; }
    virtual bool initialize(ICore* core) = 0;
    virtual void shutdown() = 0;
    virtual QVector<IViewFactory*> viewFactories() const { return {}; }
};

} // namespace est

Q_DECLARE_INTERFACE(est::IPlugin, "org.est.IPlugin")

#endif // EST_IPLUGIN_H
