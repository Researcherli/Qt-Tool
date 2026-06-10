#ifndef EST_PROTOCOLDECODER_H
#define EST_PROTOCOLDECODER_H

#include "ProtocolTemplate.h"

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QVector>

namespace est
{

    // ---------------------------------------------------------------------------
    // 解码结果
    // ---------------------------------------------------------------------------

    /// 解码后的单个字段
    struct DecodedField
    {
        QString fieldName;          ///< 字段名
        FieldType type;             ///< 字段类型
        int offset = 0;             ///< 字节偏移
        int size = 0;               ///< 字节数
        QByteArray rawBytes;        ///< 原始字节
        QString hexValue;           ///< HEX 显示值
        QString decValue;           ///< 十进制显示值
        QString otherValue;         ///< 其他格式（ASCII / 有符号 / 浮点）
        FieldDisplay display;       ///< 主要显示格式
        QColor highlightColor;      ///< 高亮颜色
        bool matched = true;        ///< Constant 字段是否符合预期
    };

    /// 解码后的完整帧
    struct DecodedFrame
    {
        qint64 timestamp = 0;           ///< 时间戳
        QString templateName;           ///< 匹配的模板名
        QByteArray rawFrame;            ///< 原始帧数据
        QVector<DecodedField> fields;   ///< 解码后的字段
        int streamOffset = -1;           ///< 帧在本次输入字节流中的起始位置
        bool valid = false;             ///< 所有 Constant 匹配成功
        bool checksumPassed = false;    ///< 校验和验证通过
    };

    // ---------------------------------------------------------------------------
    // 解析引擎
    // ---------------------------------------------------------------------------

    /**
     * 协议解析引擎 — 注册模板后对字节流进行帧匹配和字段解码。
     *
     * 匹配策略：
     *   1. 遍历所有注册模板
     *   2. 在字节流中搜索帧头(Constant 字段)的匹配位置
     *   3. 读取长度字段确定帧长
     *   4. 逐字段解码
     *   5. 验证校验和
     */
    class ProtocolDecoder : public QObject
    {
        Q_OBJECT

    public:
        explicit ProtocolDecoder(QObject *parent = nullptr);

        /// 注册模板，返回索引
        int addTemplate(const ProtocolTemplate &tmpl);

        /// 移除模板
        void removeTemplate(int index);

        /// 清空所有模板
        void clearTemplates();

        /// 获取模板列表
        const QVector<ProtocolTemplate> &templates() const;
        int templateCount() const;

        /// 从字节流中提取所有匹配帧
        QVector<DecodedFrame> decodeAll(const QByteArray &data);

        /// 用指定模板从字节流中提取匹配帧
        QVector<DecodedFrame> decode(int templateIndex, const QByteArray &data);

    signals:
        void templateAdded(int index);
        void templateRemoved(int index);
        void templatesCleared();

    private:
        /// 用单个模板在字节流中搜索并解析帧
        QVector<DecodedFrame> decodeWithTemplate(const ProtocolTemplate &tmpl,
                                                  const QByteArray &data);

        /// 解码单个字段
        DecodedField decodeField(const FieldDef &fieldDef,
                                 const QByteArray &frameData,
                                 ByteDataInspectorService::Endian endian) const;

        /// 获取字段值的文本表示
        QString formatFieldValue(const QByteArray &bytes,
                                  FieldDisplay display,
                                  ByteDataInspectorService::Endian endian) const;

        /// 计算校验和
        QByteArray calculateChecksum(const QByteArray &data,
                                      const QString &algo) const;

        QVector<ProtocolTemplate> m_templates;
    };

} // namespace est

#endif // EST_PROTOCOLDECODER_H
