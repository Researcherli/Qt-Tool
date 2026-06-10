#include "services/ByteChecksumService.h"
#include "services/EncodingUtil.h"

#include <QCryptographicHash>

namespace est
{

    namespace
    {

        QString hexValue(quint64 value, int digits)
        {
            return EncodingUtil::hexPrefix(value, digits);
        }

        QString hashHex(QByteArrayView data, QCryptographicHash::Algorithm algorithm)
        {
            return QString::fromLatin1(QCryptographicHash::hash(data.toByteArray(), algorithm).toHex().toUpper());
        }

        quint8 checksum8(QByteArrayView data)
        {
            quint32 sum = 0;
            for (char byte : data)
            {
                sum += static_cast<uchar>(byte);
            }
            return static_cast<quint8>(sum & 0xFFU);
        }

        quint16 checksum16(QByteArrayView data)
        {
            quint32 sum = 0;
            for (char byte : data)
            {
                sum += static_cast<uchar>(byte);
            }
            return static_cast<quint16>(sum & 0xFFFFU);
        }

        quint32 checksum32(QByteArrayView data)
        {
            quint32 sum = 0;
            for (char byte : data)
            {
                sum += static_cast<uchar>(byte);
            }
            return sum;
        }

        // NOTE: CRC implementations below use bit-by-bit computation.
        // For large data sets (>1MB), consider using a lookup-table approach
        // for significant performance improvement.
        quint8 crc8(QByteArrayView data)
        {
            quint8 crc = 0x00;
            for (char byte : data)
            {
                crc ^= static_cast<uchar>(byte);
                for (int bit = 0; bit < 8; ++bit)
                {
                    crc = (crc & 0x80U) != 0U
                              ? static_cast<quint8>((crc << 1U) ^ 0x07U)
                              : static_cast<quint8>(crc << 1U);
                }
            }
            return crc;
        }

        quint16 crc16CcittFalse(QByteArrayView data)
        {
            quint16 crc = 0xFFFF;
            for (char byte : data)
            {
                crc ^= static_cast<quint16>(static_cast<uchar>(byte) << 8U);
                for (int bit = 0; bit < 8; ++bit)
                {
                    crc = (crc & 0x8000U) != 0U
                              ? static_cast<quint16>((crc << 1U) ^ 0x1021U)
                              : static_cast<quint16>(crc << 1U);
                }
            }
            return crc;
        }

        quint32 crc32Ieee(QByteArrayView data)
        {
            quint32 crc = 0xFFFFFFFFU;
            for (char byte : data)
            {
                crc ^= static_cast<uchar>(byte);
                for (int bit = 0; bit < 8; ++bit)
                {
                    crc = (crc & 1U) != 0U
                              ? (crc >> 1U) ^ 0xEDB88320U
                              : crc >> 1U;
                }
            }
            return crc ^ 0xFFFFFFFFU;
        }

    } // namespace

    QMap<QString, QString> ByteChecksumService::calculate(QByteArrayView data)
    {
        QMap<QString, QString> results;
        results.insert(QStringLiteral("CheckSum-8"), hexValue(checksum8(data), 2));
        results.insert(QStringLiteral("CheckSum-16"), hexValue(checksum16(data), 4));
        results.insert(QStringLiteral("CheckSum-32"), hexValue(checksum32(data), 8));
        results.insert(QStringLiteral("CRC-8"), hexValue(crc8(data), 2));
        results.insert(QStringLiteral("CRC-16/CCITT-FALSE"), hexValue(crc16CcittFalse(data), 4));
        results.insert(QStringLiteral("CRC-32/IEEE"), hexValue(crc32Ieee(data), 8));
        results.insert(QStringLiteral("MD5"), hashHex(data, QCryptographicHash::Md5));
        results.insert(QStringLiteral("SHA-1"), hashHex(data, QCryptographicHash::Sha1));
        return results;
    }

} // namespace est
