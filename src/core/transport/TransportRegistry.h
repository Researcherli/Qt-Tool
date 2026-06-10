#ifndef EST_TRANSPORTREGISTRY_H
#define EST_TRANSPORTREGISTRY_H

#include "transport/ITransport.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <functional>

namespace est {

/**
 * 传输注册表 — 管理传输实例的创建和查询。
 *
 * 支持工厂模式注册传输类型，按需创建实例。
 */
class TransportRegistry : public QObject {
    Q_OBJECT

public:
    using TransportFactory = std::function<ITransport*()>;

    explicit TransportRegistry(QObject* parent = nullptr);
    ~TransportRegistry() override;

    /// 注册传输工厂
    void registerFactory(const QString& type, TransportFactory factory);

    /// 创建传输实例（调用方负责生命周期管理，建议通过 setParent() 管理）
    ITransport* createTransport(const QString& type);

    /// 获取所有已注册的传输类型
    QStringList registeredTypes() const;

    /// 检查传输类型是否已注册
    bool hasType(const QString& type) const;

private:
    QHash<QString, TransportFactory> m_factories;
};

} // namespace est

#endif // EST_TRANSPORTREGISTRY_H
