#include "services/ProtocolDecoder.h"

#include <QMap>
#include <QtEndian>
#include <QtMath>
#include <cstring>

namespace est
{

    // ---------------------------------------------------------------------------
    // 工具函数 (匿名命名空间)
    // ---------------------------------------------------------------------------

    namespace
    {
        /// 将字节转换为 HEX 字符串
        QString bytesToHex(const QByteArray &data)
        {
            if (data.isEmpty())
                return QStringLiteral("--");
            QStringList parts;
            for (int i = 0; i < data.size(); ++i)
                parts << QStringLiteral("%1").arg(static_cast<quint8>(data.at(i)), 2, 16, QLatin1Char('0'));
            return parts.join(QStringLiteral(" "));
        }

        /// Convert bytes to unsigned integer.
        /// For Little Endian: bytes[0] is LSB (rightmost in data stream)
        /// For Big Endian: bytes[0] is MSB (leftmost in data stream)
        quint64 bytesToUnsigned(const QByteArray &data,
                                 ByteDataInspectorService::Endian endian)
        {
            if (data.isEmpty())
                return 0;
            if (data.size() > 8)
                return 0;

            quint64 value = 0;
            if (endian == ByteDataInspectorService::Endian::Little)
            {
                for (int i = data.size() - 1; i >= 0; --i)
                    value = (value << 8) | static_cast<quint8>(data.at(i));
            }
            else
            {
                for (int i = 0; i < data.size(); ++i)
                    value = (value << 8) | static_cast<quint8>(data.at(i));
            }
            return value;
        }

        /// 将字节转换为有符号十进制
        qint64 bytesToSigned(const QByteArray &data,
                              ByteDataInspectorService::Endian endian)
        {
            const quint64 unsignedVal = bytesToUnsigned(data, endian);
            switch (data.size())
            {
            case 1:
                return static_cast<qint8>(unsignedVal);
            case 2:
                return static_cast<qint16>(unsignedVal);
            case 4:
                return static_cast<qint32>(unsignedVal);
            case 8:
                return static_cast<qint64>(unsignedVal);
            default:
                return static_cast<qint64>(unsignedVal);
            }
        }

        /// 将字节转换为浮点数
        float bytesToFloat(const QByteArray &data,
                            ByteDataInspectorService::Endian endian)
        {
            if (data.size() < 4)
                return 0.0F;

            quint32 bits = static_cast<quint32>(bytesToUnsigned(data.left(4), endian));
            float value = 0.0F;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }

        /// 校验算法 — XOR
        quint8 calcXor(const QByteArray &data)
        {
            quint8 result = 0;
            for (int i = 0; i < data.size(); ++i)
                result ^= static_cast<quint8>(data.at(i));
            return result;
        }

        /// 校验算法 — 累加和
        quint8 calcSum(const QByteArray &data)
        {
            quint32 sum = 0;
            for (int i = 0; i < data.size(); ++i)
                sum += static_cast<quint8>(data.at(i));
            return static_cast<quint8>(sum & 0xFF);
        }

        /// 校验算法 — CRC8 (多项式 0x07)
        quint8 calcCrc8(const QByteArray &data)
        {
            quint8 crc = 0;
            for (int i = 0; i < data.size(); ++i)
            {
                crc ^= static_cast<quint8>(data.at(i));
                for (int j = 0; j < 8; ++j)
                {
                    if (crc & 0x80)
                        crc = static_cast<quint8>((crc << 1) ^ 0x07);
                    else
                        crc <<= 1;
                }
            }
            return crc;
        }

    } // namespace

    // ---------------------------------------------------------------------------
    // ProtocolDecoder
    // ---------------------------------------------------------------------------

    ProtocolDecoder::ProtocolDecoder(QObject *parent)
        : QObject(parent)
    {
    }

    int ProtocolDecoder::addTemplate(const ProtocolTemplate &tmpl)
    {
        if (!tmpl.isValid())
            return -1;

        m_templates.append(tmpl);
        const int idx = m_templates.size() - 1;
        emit templateAdded(idx);
        return idx;
    }

    void ProtocolDecoder::removeTemplate(int index)
    {
        if (index < 0 || index >= m_templates.size())
            return;
        emit templateRemoved(index);
        m_templates.removeAt(index);
    }

    void ProtocolDecoder::clearTemplates()
    {
        m_templates.clear();
        emit templatesCleared();
    }

    const QVector<ProtocolTemplate> &ProtocolDecoder::templates() const
    {
        return m_templates;
    }

    int ProtocolDecoder::templateCount() const
    {
        return m_templates.size();
    }

    QVector<DecodedFrame> ProtocolDecoder::decodeAll(const QByteArray &data)
    {
        if (data.isEmpty())
            return {};

        QVector<DecodedFrame> allFrames;

        for (const auto &tmpl : m_templates)
        {
            const auto frames = decodeWithTemplate(tmpl, data);
            allFrames.append(frames);
        }

        // 按时间戳排序
        std::sort(allFrames.begin(), allFrames.end(),
                  [](const DecodedFrame &a, const DecodedFrame &b)
                  {
                      if (a.streamOffset != b.streamOffset)
                          return a.streamOffset < b.streamOffset;
                      return a.timestamp < b.timestamp;
                  });

        return allFrames;
    }

    QVector<DecodedFrame> ProtocolDecoder::decode(int templateIndex, const QByteArray &data)
    {
        if (templateIndex < 0 || templateIndex >= m_templates.size())
            return decodeAll(data);
        return decodeWithTemplate(m_templates[templateIndex], data);
    }

    QVector<DecodedFrame> ProtocolDecoder::decodeWithTemplate(
        const ProtocolTemplate &tmpl, const QByteArray &data)
    {
        QVector<DecodedFrame> frames;

        if (data.isEmpty() || tmpl.fields.isEmpty())
            return frames;

        const qint64 now = QDateTime::currentMSecsSinceEpoch();

        // 查找第一个 Constant 字段（帧头）
        int frameStartFieldIdx = -1;
        QByteArray frameHeader;
        for (int i = 0; i < tmpl.fields.size(); ++i)
        {
            if (tmpl.fields[i].type == FieldType::Constant && !tmpl.fields[i].expectedValue.isEmpty())
            {
                frameStartFieldIdx = i;
                frameHeader = tmpl.fields[i].expectedValue;
                break;
            }
        }

        if (frameStartFieldIdx < 0)
        {
            // 没有 Constant 字段，从 data[0] 开始按固定长度解析
            int frameSize = 0;
            for (const auto &f : tmpl.fields)
            {
                if (f.size <= 0)
                    return frames;
                frameSize = qMax(frameSize, f.offset + f.size);
            }

            if (frameSize <= 0 || frameSize > tmpl.maxFrameSize)
                return frames;

            for (int pos = 0; pos + frameSize <= data.size(); pos += frameSize)
            {
                const QByteArray frameData = data.mid(pos, frameSize);
                DecodedFrame frame;
                frame.timestamp = now;
                frame.templateName = tmpl.name;
                frame.rawFrame = frameData;
                frame.streamOffset = pos;
                frame.valid = true;
                frame.checksumPassed = tmpl.checksumAlgo.isEmpty();

                for (const auto &fieldDef : tmpl.fields)
                {
                    DecodedField df = decodeField(fieldDef, frameData, tmpl.endian);
                    if (fieldDef.type == FieldType::Constant && !fieldDef.expectedValue.isEmpty())
                    {
                        df.matched = (df.rawBytes == fieldDef.expectedValue);
                        if (!df.matched)
                            frame.valid = false;
                    }
                    frame.fields.append(df);
                }

                if (!tmpl.checksumAlgo.isEmpty())
                {
                    const auto checksumIt = std::find_if(frame.fields.cbegin(), frame.fields.cend(),
                                                         [](const DecodedField &field) {
                                                             return field.type == FieldType::Checksum;
                                                         });
                    if (checksumIt != frame.fields.cend())
                    {
                        // 跳过 checksum 区间计算校验和，避免 remove() 导致偏移错位
                        QByteArray dataForChecksum;
                        if (checksumIt->offset > 0)
                            dataForChecksum.append(frameData.left(checksumIt->offset));
                        if (checksumIt->offset + checksumIt->size < frameData.size())
                            dataForChecksum.append(frameData.mid(checksumIt->offset + checksumIt->size));
                        frame.checksumPassed = (checksumIt->rawBytes == calculateChecksum(dataForChecksum, tmpl.checksumAlgo));
                        frame.valid = frame.valid && frame.checksumPassed;
                    }
                    else
                    {
                        frame.valid = false;
                    }
                }

                frames.append(frame);
            }
            return frames;
        }

        // 有帧头：在 data 中搜索帧头
        int searchPos = 0;
        const int headerSize = frameHeader.size();

        while (true)
        {
            const int headerPos = data.indexOf(frameHeader, searchPos);
            if (headerPos < 0)
                break;

            const int frameStart = headerPos - tmpl.fields[frameStartFieldIdx].offset;
            if (frameStart < 0)
            {
                searchPos = headerPos + 1;
                continue;
            }

            // 计算帧总长
            int frameSize = 0;
            bool hasAutoPayload = false;

            for (int i = 0; i < tmpl.fields.size(); ++i)
            {
                const auto &f = tmpl.fields[i];
                if (f.type == FieldType::Payload && f.size == 0)
                {
                    hasAutoPayload = true;
                    continue;
                }
                if (f.type == FieldType::Checksum && f.offset < 0)
                {
                    // Checksum 在最后，自动计算偏移
                    continue;
                }
                frameSize = qMax(frameSize, f.offset + f.size);
            }

            // 如果有 Length 字段，用它来确定帧长
            int lengthValue = 0;
            for (const auto &f : tmpl.fields)
            {
                if (f.type == FieldType::Length)
                {
                    const int lenOffset = frameStart + f.offset;
                    if (lenOffset + f.size <= data.size())
                    {
                        lengthValue = static_cast<int>(
                            bytesToUnsigned(data.mid(lenOffset, f.size), tmpl.endian));
                    }
                    break;
                }
            }

            if (lengthValue > 0)
                frameSize = lengthValue;
            else if (hasAutoPayload)
            {
                // 变长帧：尝试通过后续字段推断帧长，或使用尾部匹配
                int tailOffset = 0;
                for (int i = 0; i < tmpl.fields.size(); ++i)
                {
                    const auto &f = tmpl.fields[i];
                    if (f.type == FieldType::Checksum && f.offset < 0 && f.size > 0)
                    {
                        // Checksum 在帧尾，帧长 = 最后一个非 Payload 字段末尾 + checksum
                        tailOffset = qMax(tailOffset, f.size);
                    }
                    else if (f.type != FieldType::Payload && f.offset >= 0 && f.size > 0)
                    {
                        // 非 Payload 字段的末尾位置作为最小帧长参考
                        tailOffset = qMax(tailOffset, f.offset + f.size);
                    }
                }

                // 从帧头位置开始，在剩余数据中搜索下一个帧头以确定帧边界
                const int nextHeaderPos = data.indexOf(frameHeader, headerPos + headerSize);
                if (nextHeaderPos > frameStart)
                {
                    // 下一个帧头之前就是当前帧的范围，减去可能的 checksum
                    frameSize = nextHeaderPos - frameStart;
                    if (tailOffset > 0 && frameSize > tailOffset)
                        frameSize -= 0; // 保留完整帧长，checksum 在尾部
                }
                else if (tailOffset > 0)
                {
                    // 没有下一个帧头，使用尾部偏移作为最小帧长
                    frameSize = tailOffset;
                }
                else
                {
                    frameSize = -1; // 无法确定帧长
                }
            }

            // 检查边界
            if (frameSize <= 0 || frameSize > tmpl.maxFrameSize)
            {
                searchPos = headerPos + 1;
                continue;
            }

            if (frameStart + frameSize > data.size())
            {
                break;
            }

            const QByteArray frameData = data.mid(frameStart, frameSize);
            DecodedFrame frame;
            frame.timestamp = now;
            frame.templateName = tmpl.name;
            frame.rawFrame = frameData;
            frame.streamOffset = frameStart;
            frame.valid = true;
            frame.checksumPassed = tmpl.checksumAlgo.isEmpty();

            // 逐字段解码
            for (int i = 0; i < tmpl.fields.size(); ++i)
            {
                const auto &fieldDef = tmpl.fields[i];

                // 计算字段偏移和大小
                int fOffset = fieldDef.offset;
                int fSize = fieldDef.size;

                if (fieldDef.type == FieldType::Payload && fSize == 0)
                {
                    int nextOffset = frameSize;
                    for (int j = 0; j < tmpl.fields.size(); ++j)
                    {
                        if (j == i)
                            continue;
                        const auto &candidate = tmpl.fields[j];
                        if (candidate.offset >= 0 && candidate.offset > fOffset)
                            nextOffset = qMin(nextOffset, candidate.offset);
                        if (candidate.type == FieldType::Checksum && candidate.offset < 0 && candidate.size > 0)
                            nextOffset = qMin(nextOffset, frameSize - candidate.size);
                    }
                    fSize = nextOffset - fOffset;
                }

                if (fieldDef.type == FieldType::Checksum && fieldDef.offset < 0)
                {
                    // Checksum 自动放在最后：偏移 = 帧长 - 大小
                    fOffset = (fSize > 0) ? frameSize - fSize : -1;
                }

                if (fOffset < 0)
                    continue;
                if (fOffset + fSize > frameSize)
                    fSize = frameSize - fOffset;
                if (fSize <= 0)
                    continue;

                FieldDef adjusted = fieldDef;
                adjusted.offset = fOffset;
                adjusted.size = fSize;

                DecodedField df = decodeField(adjusted, frameData, tmpl.endian);

                // 验证 Constant 字段
                if (fieldDef.type == FieldType::Constant && !fieldDef.expectedValue.isEmpty())
                {
                    const QByteArray actual = frameData.mid(fOffset, qMin(fSize, fieldDef.expectedValue.size()));
                    df.matched = (actual == fieldDef.expectedValue);
                    if (!df.matched)
                        frame.valid = false;
                }

                frame.fields.append(df);
            }

            // 校验和验证
            if (!tmpl.checksumAlgo.isEmpty())
            {
                // 找到实际的 Checksum 字段字节
                bool checksumFieldFound = false;
                for (const auto &df : frame.fields)
                {
                    if (df.type == FieldType::Checksum)
                    {
                        checksumFieldFound = true;
                        // 跳过 checksum 区间计算校验和，避免 remove() 导致偏移错位
                        QByteArray dataForChecksum;
                        if (df.offset > 0)
                            dataForChecksum.append(frame.rawFrame.left(df.offset));
                        if (df.offset + df.size < frame.rawFrame.size())
                            dataForChecksum.append(frame.rawFrame.mid(df.offset + df.size));
                        const QByteArray expectedChecksum = calculateChecksum(dataForChecksum, tmpl.checksumAlgo);
                        frame.checksumPassed = (df.rawBytes == expectedChecksum);
                        if (!frame.checksumPassed)
                        {
                            frame.valid = false;
                        }
                        break;
                    }
                }
                if (!checksumFieldFound)
                    frame.valid = false;
            }

            frames.append(frame);
            searchPos = frameStart + qMax(1, frameSize);
        }

        return frames;
    }

    DecodedField ProtocolDecoder::decodeField(const FieldDef &fieldDef,
                                               const QByteArray &frameData,
                                               ByteDataInspectorService::Endian endian) const
    {
        DecodedField df;
        df.fieldName = fieldDef.name;
        df.type = fieldDef.type;
        df.offset = fieldDef.offset;
        df.size = fieldDef.size;
        df.display = fieldDef.display;
        df.highlightColor = fieldDef.highlightColor;
        df.matched = true;

        const int offset = fieldDef.offset;
        const int size = qMin(fieldDef.size, frameData.size() - offset);

        if (offset < 0 || offset >= frameData.size() || size <= 0)
        {
            df.hexValue = QStringLiteral("--");
            df.decValue = QStringLiteral("--");
            return df;
        }

        df.rawBytes = frameData.mid(offset, size);

        // 根据显示格式生成文本
        df.hexValue = bytesToHex(df.rawBytes);
        df.decValue = QString::number(bytesToUnsigned(df.rawBytes, endian));

        // 其他格式
        switch (fieldDef.display)
        {
        case FieldDisplay::Hex:
            break;
        case FieldDisplay::Dec:
            df.otherValue = QStringLiteral("0x%1").arg(
                bytesToUnsigned(df.rawBytes, endian), size * 2, 16, QLatin1Char('0'));
            break;
        case FieldDisplay::Signed:
            df.otherValue = QString::number(
                bytesToSigned(df.rawBytes, endian));
            break;
        case FieldDisplay::Bin:
        {
            QString bin;
            for (int i = 0; i < df.rawBytes.size(); ++i)
            {
                if (i > 0)
                    bin += QStringLiteral(" ");
                bin += QStringLiteral("%1").arg(static_cast<quint8>(df.rawBytes.at(i)), 8, 2, QLatin1Char('0'));
            }
            df.otherValue = bin;
            break;
        }
        case FieldDisplay::ASCII:
        {
            QString ascii;
            for (int i = 0; i < df.rawBytes.size(); ++i)
            {
                const char c = df.rawBytes.at(i);
                if (c >= 32 && c < 127)
                    ascii += c;
                else
                    ascii += QStringLiteral(".");
            }
            df.otherValue = ascii;
            break;
        }
        case FieldDisplay::Float:
        {
            const float f = bytesToFloat(df.rawBytes, endian);
            df.otherValue = QString::number(static_cast<double>(f), 'g', 6);
            break;
        }
        }

        return df;
    }

    QString ProtocolDecoder::formatFieldValue(
        const QByteArray &bytes,
        FieldDisplay display,
        ByteDataInspectorService::Endian endian) const
    {
        if (bytes.isEmpty())
            return QStringLiteral("--");

        switch (display)
        {
        case FieldDisplay::Hex:
            return bytesToHex(bytes);
        case FieldDisplay::Dec:
            return QString::number(bytesToUnsigned(bytes, endian));
        case FieldDisplay::Signed:
            return QString::number(bytesToSigned(bytes, endian));
        case FieldDisplay::Bin:
        {
            QString bin;
            for (int i = 0; i < bytes.size(); ++i)
                bin += QStringLiteral("%1").arg(static_cast<quint8>(bytes.at(i)), 8, 2, QLatin1Char('0'));
            return bin;
        }
        case FieldDisplay::ASCII:
        {
            QString ascii;
            for (int i = 0; i < bytes.size(); ++i)
            {
                const char c = bytes.at(i);
                ascii += (c >= 32 && c < 127) ? c : QChar('.');
            }
            return ascii;
        }
        case FieldDisplay::Float:
        {
            if (bytes.size() < 4)
                return QStringLiteral("--");
            return QString::number(bytesToFloat(bytes, endian), 'g', 6);
        }
        }
        return bytesToHex(bytes);
    }

    QByteArray ProtocolDecoder::calculateChecksum(const QByteArray &data,
                                                    const QString &algo) const
    {
        // TODO: 未来可统一使用 ByteChecksumService 进行校验和计算，
        // 这里的 calcXor / calcSum / calcCrc8 为匿名命名空间内的局部实现，
        // 针对协议解码单字节结果场景，与 ByteChecksumService 的多字节接口有所不同。
        if (algo == QStringLiteral("xor"))
        {
            const quint8 val = calcXor(data);
            return QByteArray(1, static_cast<char>(val));
        }
        if (algo == QStringLiteral("sum"))
        {
            const quint8 val = calcSum(data);
            return QByteArray(1, static_cast<char>(val));
        }
        if (algo == QStringLiteral("crc8"))
        {
            const quint8 val = calcCrc8(data);
            return QByteArray(1, static_cast<char>(val));
        }
        return QByteArray();
    }

} // namespace est
