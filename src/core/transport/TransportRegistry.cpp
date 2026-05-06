#include "transport/TransportRegistry.h"

namespace est {

TransportRegistry::TransportRegistry(QObject* parent)
    : QObject(parent)
{
}

TransportRegistry::~TransportRegistry() = default;

void TransportRegistry::registerFactory(const QString& type, TransportFactory factory)
{
    m_factories.insert(type, std::move(factory));
}

ITransport* TransportRegistry::createTransport(const QString& type)
{
    auto it = m_factories.find(type);
    if (it == m_factories.end()) {
        qWarning() << "Transport type not registered:" << type;
        return nullptr;
    }
    return it.value()();
}

QStringList TransportRegistry::registeredTypes() const
{
    return m_factories.keys();
}

bool TransportRegistry::hasType(const QString& type) const
{
    return m_factories.contains(type);
}

} // namespace est
