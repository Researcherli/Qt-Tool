#ifndef EST_DATABUS_H
#define EST_DATABUS_H

#include "DataPacket.h"
#include "SubscriptionHandle.h"

#include <QHash>
#include <QObject>
#include <QReadWriteLock>
#include <QString>
#include <QVector>
#include <functional>

namespace est {

/**
 * 数据总线 — 模块间通信的核心中枢。
 *
 * 发布/订阅模式，通过通道字符串路由数据，生产者与消费者完全解耦。
 *
 * 通道命名规范：
 *   transport.<type>.<instance>  — 传输层原始数据
 *   protocol.<type>.<channel>    — 解码后协议数据
 *   plugin.<name>.<signal>       — 插件自定义信号
 */
class DataBus : public QObject {
    Q_OBJECT

public:
    explicit DataBus(QObject* parent = nullptr);
    ~DataBus() override;

    /// 发布数据到指定通道（线程安全，可通过信号跨线程）
    void publish(const QString& channel, const DataPacket& packet);

    /// 订阅通道，返回取消订阅的句柄
    SubscriptionHandle subscribe(const QString& channel,
                                  std::function<void(const DataPacket&)> handler);

    /// 取消订阅
    void unsubscribe(const SubscriptionHandle& handle);

    /// 取消指定通道的所有订阅
    void unsubscribeAll(const QString& channel);

    /// 获取通道的订阅者数量（调试用）
    int subscriberCount(const QString& channel) const;

    /// 获取所有活跃通道名
    QStringList activeChannels() const;

signals:
    /// 数据发布信号（可用于跨线程通信或调试）
    void dataPublished(const QString& channel, const est::DataPacket& packet);

private:
    struct Subscriber {
        SubscriptionHandle handle;
        std::function<void(const DataPacket&)> handler;
    };

    QHash<QString, QVector<Subscriber>> m_subscribers;
    QHash<quint64, QString> m_handleToChannel;
    quint64 m_nextHandleId = 1;
    mutable QReadWriteLock m_lock;
};

} // namespace est

#endif // EST_DATABUS_H
