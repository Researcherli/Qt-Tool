#ifndef EST_DATAPACKET_H
#define EST_DATAPACKET_H

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVariantMap>
#include <QtGlobal>

namespace est {

/**
 * 数据包 — 系统中所有数据流的基本传输单元。
 *
 * 通过 DataBus 在模块间传递，携带源时间戳、通道名、原始载荷和扩展元数据。
 * QByteArray 和 QVariantMap 本身使用 COW，高频场景下无需额外共享包装。
 */
struct DataPacket {
    qint64 timestamp = 0;        ///< 毫秒级时间戳（相对或绝对，由数据源决定）
    QString channel;             ///< 来源通道（如 "transport.can.0"）
    QByteArray rawPayload;       ///< 原始字节载荷
    QVariantMap metadata;        ///< 扩展元数据（CAN ID, DLC, 方向等）

    bool isValid() const { return !channel.isEmpty(); }

    // 便捷构造：CAN 帧
    static DataPacket makeCanFrame(int canId, const QByteArray& data,
                                    int dlc = -1, qint64 ts = 0,
                                    const QString& ch = "transport.can.0") {
        DataPacket p;
        p.timestamp = ts;
        p.channel = ch;
        p.rawPayload = data;
        p.metadata["can_id"] = canId;
        p.metadata["dlc"] = (dlc < 0) ? data.size() : dlc;
        p.metadata["direction"] = QStringLiteral("rx");
        return p;
    }
};

} // namespace est

Q_DECLARE_METATYPE(est::DataPacket)

#endif // EST_DATAPACKET_H
