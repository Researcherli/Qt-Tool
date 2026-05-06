#include "DataBus.h"

#include <QDebug>
#include <algorithm>

namespace est {

DataBus::DataBus(QObject* parent)
    : QObject(parent)
{
}

DataBus::~DataBus() = default;

void DataBus::publish(const QString& channel, const DataPacket& packet)
{
    QReadLocker locker(&m_lock);
    auto it = m_subscribers.find(channel);
    if (it == m_subscribers.end()) {
        return; // 无订阅者，静默丢弃
    }

    const QVector<Subscriber> subs = it.value();
    locker.unlock(); // 提前释放读锁，防止回调中死锁

    for (const Subscriber& sub : subs) {
        if (sub.handler) {
            sub.handler(packet);
        }
    }

    emit dataPublished(channel, packet);
}

SubscriptionHandle DataBus::subscribe(const QString& channel,
                                       std::function<void(const DataPacket&)> handler)
{
    QWriteLocker locker(&m_lock);
    SubscriptionHandle handle(++m_nextHandleId);
    m_subscribers[channel].append({handle, std::move(handler)});
    m_handleToChannel[handle.id()] = channel;
    return handle;
}

void DataBus::unsubscribe(const SubscriptionHandle& handle)
{
    QWriteLocker locker(&m_lock);
    auto chanIt = m_handleToChannel.find(handle.id());
    if (chanIt == m_handleToChannel.end()) {
        return;
    }

    QString channel = chanIt.value();
    m_handleToChannel.erase(chanIt);

    auto subIt = m_subscribers.find(channel);
    if (subIt != m_subscribers.end()) {
        auto& subs = subIt.value();
        subs.erase(std::remove_if(subs.begin(), subs.end(),
                                   [&handle](const Subscriber& s) {
                                       return s.handle == handle;
                                   }),
                    subs.end());

        if (subs.isEmpty()) {
            m_subscribers.erase(subIt);
        }
    }
}

void DataBus::unsubscribeAll(const QString& channel)
{
    QWriteLocker locker(&m_lock);
    auto it = m_subscribers.find(channel);
    if (it == m_subscribers.end()) {
        return;
    }

    // 清除 handle → channel 映射
    const QVector<Subscriber>& subs = it.value();
    for (const Subscriber& sub : subs) {
        m_handleToChannel.remove(sub.handle.id());
    }

    m_subscribers.erase(it);
}

int DataBus::subscriberCount(const QString& channel) const
{
    QReadLocker locker(&m_lock);
    auto it = m_subscribers.find(channel);
    return (it != m_subscribers.end()) ? it.value().size() : 0;
}

QStringList DataBus::activeChannels() const
{
    QReadLocker locker(&m_lock);
    return m_subscribers.keys();
}

} // namespace est
