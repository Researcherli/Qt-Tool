#ifndef EST_ITRANSPORT_H
#define EST_ITRANSPORT_H

#include "databus/DataPacket.h"

#include <QObject>
#include <QVariantMap>

namespace est {

/**
 * 传输接口 — 所有数据来源的统一抽象。
 *
 * 无论是文件回放、模拟器还是真实硬件，都实现此接口。
 * 上层通过 TransportRegistry 按名称获取实例，不关心具体实现。
 */
class ITransport : public QObject {
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        Error
    };
    Q_ENUM(State)

    explicit ITransport(QObject* parent = nullptr) : QObject(parent) {}
    ~ITransport() override = default;

    /// 传输类型标识（如 "file-replay", "serial", "can-adapter"）
    virtual QString transportType() const = 0;

    /// 传输实例名称（如 "CAN Channel 0", "COM3"）
    virtual QString name() const = 0;

    /// 当前连接状态
    virtual State state() const = 0;

    /// 打开传输（配置通过参数传入）
    virtual bool open(const QVariantMap& config) = 0;

    /// 关闭传输
    virtual void close() = 0;

    /// 发送数据包
    virtual bool send(const DataPacket& packet) = 0;

signals:
    /// 收到数据
    void dataReceived(const est::DataPacket& packet);

    /// 状态变更
    void stateChanged(est::ITransport::State newState);

    /// 错误
    void errorOccurred(const QString& errorMessage);
};

} // namespace est

#endif // EST_ITRANSPORT_H
