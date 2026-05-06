#ifndef EST_SUBSCRIPTIONHANDLE_H
#define EST_SUBSCRIPTIONHANDLE_H

#include <QMetaType>
#include <QtGlobal>

namespace est
{

    /**
     * 订阅句柄 — 由 DataBus::subscribe() 返回，用于取消订阅。
     */
    class SubscriptionHandle
    {
    public:
        SubscriptionHandle() = default;
        explicit SubscriptionHandle(quint64 id) : m_id(id) {}

        bool isValid() const { return m_id != 0; }
        bool operator==(const SubscriptionHandle &other) const { return m_id == other.m_id; }
        bool operator!=(const SubscriptionHandle &other) const { return m_id != other.m_id; }

        quint64 id() const { return m_id; }

    private:
        quint64 m_id = 0;
    };

} // namespace est

Q_DECLARE_METATYPE(est::SubscriptionHandle)

#endif // EST_SUBSCRIPTIONHANDLE_H
