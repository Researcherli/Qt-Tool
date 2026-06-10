#include "DataBus.h"

#include <QDebug>
#include <algorithm>

namespace est {

DataBus::DataBus(QObject* parent)
    : QObject(parent)
{
    // m_nextHandleId starts at 1, quint64 overflow is practically impossible
    // but adding a safety check doesn't hurt
}

DataBus::~DataBus() = default;

void DataBus::publish(const QString& channel, const DataPacket& packet)
{
    QReadLocker locker(&m_lock);

    QVector<Subscriber> subs;
    subs.reserve(m_subscribers.size());  // Estimate capacity
    for (auto it = m_subscribers.cbegin(); it != m_subscribers.cend(); ++it) {
        if (channelMatches(it.key(), channel)) {
            subs += it.value();
        }
    }

    locker.unlock(); // 提前释放读锁，防止回调中死锁

    for (const Subscriber& sub : subs) {
        if (sub.handler) {
            sub.handler(packet);
        }
    }

    // Observers such as the global data logger must see every publication,
    // even when no module is currently subscribed to that channel.
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

bool DataBus::channelMatches(const QString& subscriptionChannel, const QString& publishedChannel)
{
    // Supports "foo.*" prefix wildcard only (not "*.bar" or "foo.*.baz")
    if (subscriptionChannel == publishedChannel) {
        return true;
    }

    if (subscriptionChannel.endsWith(QStringLiteral(".*"))) {
        const QString prefix = subscriptionChannel.left(subscriptionChannel.size() - 1);
        return publishedChannel.startsWith(prefix);
    }

    return false;
}

} // namespace est
