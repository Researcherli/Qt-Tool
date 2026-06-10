#include "services/ByteDataInspectorService.h"

#include <QLocale>

#include <cstring>

namespace est
{

    namespace
    {

        bool hasBytes(QByteArrayView data, qsizetype offset, qsizetype width)
        {
            return offset >= 0 && width >= 0 && offset <= data.size() && width <= data.size() - offset;
        }

        quint64 readUnsigned(QByteArrayView data,
                             qsizetype offset,
                             qsizetype width,
                             ByteDataInspectorService::Endian endian)
        {
            if (width <= 0 || width > 8) {
                qWarning() << "Invalid width for readUnsigned:" << width;
                return 0;
            }
            quint64 value = 0;
            if (endian == ByteDataInspectorService::Endian::Little)
            {
                for (qsizetype index = 0; index < width; ++index)
                {
                    value |= static_cast<quint64>(static_cast<uchar>(data.at(offset + index))) << (index * 8);
                }
            }
            else
            {
                for (qsizetype index = 0; index < width; ++index)
                {
                    value = (value << 8) | static_cast<uchar>(data.at(offset + index));
                }
            }
            return value;
        }

        QString unsignedHex(quint64 value, int digits)
        {
            return QStringLiteral("0x%1").arg(value, digits, 16, QLatin1Char('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x"));
        }

        QString formatUnsigned(QByteArrayView data,
                               qsizetype offset,
                               qsizetype width,
                               ByteDataInspectorService::Endian endian)
        {
            if (!hasBytes(data, offset, width))
            {
                return QStringLiteral("-");
            }
            const quint64 value = readUnsigned(data, offset, width, endian);
            return QStringLiteral("%1 (%2)").arg(value).arg(unsignedHex(value, static_cast<int>(width * 2)));
        }

        QString formatSigned(QByteArrayView data,
                             qsizetype offset,
                             qsizetype width,
                             ByteDataInspectorService::Endian endian)
        {
            if (!hasBytes(data, offset, width))
            {
                return QStringLiteral("-");
            }

            const quint64 unsignedValue = readUnsigned(data, offset, width, endian);
            qint64 signedValue = 0;
            switch (width)
            {
            case 1:
                signedValue = static_cast<qint8>(unsignedValue);
                break;
            case 2:
                signedValue = static_cast<qint16>(unsignedValue);
                break;
            case 4:
                signedValue = static_cast<qint32>(unsignedValue);
                break;
            case 8:
                signedValue = static_cast<qint64>(unsignedValue);
                break;
            default:
                return QStringLiteral("-%1 (不支持 %2 字节有符号)").arg(unsignedHex(unsignedValue, static_cast<int>(width * 2))).arg(width);
            }

            return QStringLiteral("%1 (%2)").arg(signedValue).arg(unsignedHex(unsignedValue, static_cast<int>(width * 2)));
        }

        QString formatFloat(QByteArrayView data,
                            qsizetype offset,
                            ByteDataInspectorService::Endian endian)
        {
            if (!hasBytes(data, offset, 4))
            {
                return QStringLiteral("-");
            }

            quint32 bits = static_cast<quint32>(readUnsigned(data, offset, 4, endian));
            float value = 0.0F;
            std::memcpy(&value, &bits, sizeof(value));
            return QLocale::c().toString(value, 'g', 8);
        }

        QString formatDouble(QByteArrayView data,
                             qsizetype offset,
                             ByteDataInspectorService::Endian endian)
        {
            if (!hasBytes(data, offset, 8))
            {
                return QStringLiteral("-");
            }

            quint64 bits = readUnsigned(data, offset, 8, endian);
            double value = 0.0;
            std::memcpy(&value, &bits, sizeof(value));
            return QLocale::c().toString(value, 'g', 12);
        }

    } // namespace

    QMap<QString, QString> ByteDataInspectorService::inspect(QByteArrayView data, qsizetype offset, Endian endian)
    {
        QMap<QString, QString> rows;
        rows.insert(QStringLiteral("int8"), formatSigned(data, offset, 1, endian));
        rows.insert(QStringLiteral("uint8"), formatUnsigned(data, offset, 1, endian));
        rows.insert(QStringLiteral("int16"), formatSigned(data, offset, 2, endian));
        rows.insert(QStringLiteral("uint16"), formatUnsigned(data, offset, 2, endian));
        rows.insert(QStringLiteral("int32"), formatSigned(data, offset, 4, endian));
        rows.insert(QStringLiteral("uint32"), formatUnsigned(data, offset, 4, endian));
        rows.insert(QStringLiteral("int64"), formatSigned(data, offset, 8, endian));
        rows.insert(QStringLiteral("uint64"), formatUnsigned(data, offset, 8, endian));
        rows.insert(QStringLiteral("float"), formatFloat(data, offset, endian));
        rows.insert(QStringLiteral("double"), formatDouble(data, offset, endian));
        return rows;
    }

} // namespace est
