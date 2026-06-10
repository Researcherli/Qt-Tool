#ifndef EST_PROTOCOLTEMPLATE_H
#define EST_PROTOCOLTEMPLATE_H

#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QVector>

#include "services/ByteDataInspectorService.h"

namespace est
{

    // ---------------------------------------------------------------------------
    // 字段类型
    // ---------------------------------------------------------------------------

    enum class FieldType
    {
        Constant,   ///< 固定值，用于帧头匹配
        Length,     ///< 帧长度字段
        Type,       ///< 帧类型/命令码
        Address,    ///< 地址
        Payload,    ///< 数据负载（可变长度）
        Checksum,   ///< 校验和
        Sequence,   ///< 序列号
        Status,     ///< 状态
        Reserved,   ///< 保留/填充
        Custom      ///< 自定义
    };

    /// 字段类型 → 中文名称
    QString fieldTypeToString(FieldType type);

    /// 字段显示格式
    enum class FieldDisplay
    {
        Hex,    ///< 十六进制
        Dec,    ///< 无符号十进制
        Signed, ///< 有符号十进制
        Bin,    ///< 二进制
        ASCII,  ///< ASCII 字符
        Float   ///< 浮点数
    };

    QString fieldDisplayToString(FieldDisplay display);

    // ---------------------------------------------------------------------------
    // 字段定义
    // ---------------------------------------------------------------------------

    struct FieldDef
    {
        QString name;                       ///< 字段名（如"帧头"、"长度"）
        QString description;                ///< 字段说明
        FieldType type = FieldType::Custom;
        int offset = 0;                     ///< 相对帧起始的偏移
        int size = 1;                       ///< 字节数（0 = 自动从长度字段计算）
        QByteArray expectedValue;           ///< Constant 字段的期望值
        FieldDisplay display = FieldDisplay::Hex;
        QColor highlightColor = QColor(QStringLiteral("#E3F2FD"));

        QJsonObject toJson() const;
        static FieldDef fromJson(const QJsonObject &obj);
    };

    // ---------------------------------------------------------------------------
    // 协议模板
    // ---------------------------------------------------------------------------

    struct ProtocolTemplate
    {
        QString name;                       ///< 模板名（如"UART通信协议"）
        QString description;                ///< 模板说明
        QVector<FieldDef> fields;           ///< 字段列表（按偏移序）
        int maxFrameSize = 256;             ///< 最大帧长度
        ByteDataInspectorService::Endian endian = ByteDataInspectorService::Endian::Little;
        QString checksumAlgo;               ///< 校验算法：空/xor/sum/crc8

        /// 验证模板是否有效
        bool isValid() const;

        QJsonObject toJson() const;
        static ProtocolTemplate fromJson(const QJsonObject &obj);

        /// 批量保存/加载到 QSettings
        static void saveList(const QVector<ProtocolTemplate> &list);
        static QVector<ProtocolTemplate> loadList();
    };

} // namespace est

#endif // EST_PROTOCOLTEMPLATE_H
