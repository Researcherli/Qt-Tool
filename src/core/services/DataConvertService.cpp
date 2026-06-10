#include "services/DataConvertService.h"

#include "services/ByteFormatService.h"
#include "services/EncodingUtil.h"

#include <QChar>
#include <QRegularExpressionMatch>
#include <QRegularExpression>
#include <QStringList>

namespace est
{

    namespace
    {

        QString decodeGbk(const QByteArray &bytes, bool *ok, QString *errorMessage)
        {
            return EncodingUtil::decodeGbk(bytes, ok, errorMessage);
        }

        QByteArray encodeGbk(const QString &text, bool *ok, QString *errorMessage)
        {
            return EncodingUtil::encodeGbk(text, ok, errorMessage);
        }

        QString decodeUtf16(const QByteArray &bytes, bool bigEndian, bool *ok, QString *errorMessage)
        {
            return EncodingUtil::decodeUtf16(bytes, bigEndian, ok, errorMessage);
        }

        QByteArray encodeUtf16(const QString &text, bool bigEndian)
        {
            return EncodingUtil::encodeUtf16(text, bigEndian);
        }

        QString normalizedHexInput(QString input)
        {
            return EncodingUtil::normalizedHexInput(input);
        }

        QString sanitizedArrayName(const QString &arrayName)
        {
            return EncodingUtil::sanitizedArrayName(arrayName);
        }

        QString normalizedBase64Input(QString input)
        {
            input.remove(QRegularExpression(QStringLiteral("\\s+")));
            return input;
        }

    } // namespace

    bool DataConvertService::convert(const QString &input,
                                     const ConversionOptions &options,
                                     QString *output,
                                     QString *errorMessage)
    {
        QByteArray bytes;
        if (!decodeToBytes(input, options.inputFormat, options.encoding, &bytes, errorMessage))
        {
            return false;
        }

        if (output != nullptr)
        {
            if (options.outputFormat == DataFormat::String) {
                *output = encodeBytes(bytes, options.outputFormat, options.encoding,
                                      options.hexSeparatorMode, options.upperCaseHex,
                                      errorMessage);
            } else {
                *output = encodeBytes(bytes, options.outputFormat, TextEncoding::UTF8,
                                      options.hexSeparatorMode, options.upperCaseHex,
                                      errorMessage);
            }
        }
        return true;
    }

    bool DataConvertService::decodeToBytes(const QString &input,
                                           DataFormat format,
                                           TextEncoding encoding,
                                           QByteArray *output,
                                           QString *errorMessage)
    {
        switch (format)
        {
        case DataFormat::String:
        {
            bool ok = false;
            const QByteArray bytes = encodeString(input, encoding, &ok, errorMessage);
            if (!ok)
            {
                return false;
            }
            if (output != nullptr)
            {
                *output = bytes;
            }
            return true;
        }
        case DataFormat::Hex:
            return parseHex(input, output, errorMessage);
        case DataFormat::Binary:
            return parseBinary(input, output, errorMessage);
        case DataFormat::Base64:
            return parseBase64(input, output, errorMessage);
        case DataFormat::DecimalBytes:
            return parseDecimalBytes(input, output, errorMessage);
        }

        return false;
    }

    QString DataConvertService::encodeBytes(const QByteArray &bytes,
                                            DataFormat format,
                                            TextEncoding encoding,
                                            const QString &hexSeparatorMode,
                                            bool upperCaseHex,
                                            QString *errorMessage)
    {
        switch (format)
        {
        case DataFormat::String:
            return decodeBytesToString(bytes, encoding, nullptr, errorMessage);
        case DataFormat::Hex:
            return formatHex(bytes, hexSeparatorMode, upperCaseHex);
        case DataFormat::Binary:
            return formatBinary(bytes);
        case DataFormat::Base64:
            return formatBase64(bytes);
        case DataFormat::DecimalBytes:
            return formatDecimalBytes(bytes);
        }

        if (errorMessage != nullptr)
        {
            *errorMessage = QStringLiteral("未知输出格式。");
        }
        return {};
    }

    bool DataConvertService::parseHex(const QString &input,
                                      QByteArray *output,
                                      QString *errorMessage)
    {
        return ByteFormatService::parseHex(input, output, errorMessage);
    }

    bool DataConvertService::parseBinary(const QString &input,
                                         QByteArray *output,
                                         QString *errorMessage)
    {
        return ByteFormatService::parseBinary(input, output, errorMessage);
    }

    bool DataConvertService::parseBase64(const QString &input,
                                         QByteArray *output,
                                         QString *errorMessage)
    {
        const QString normalized = normalizedBase64Input(input);
        if (normalized.isEmpty())
        {
            if (output != nullptr)
            {
                output->clear();
            }
            return true;
        }

        QByteArray bytes;
        const QByteArray encoded = normalized.toLatin1();
        const auto decoded = QByteArray::fromBase64Encoding(encoded);
        if (decoded.decodingStatus != QByteArray::Base64DecodingStatus::Ok)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("Base64 输入格式无效。\n请检查字符和填充位。");
            }
            return false;
        }

        bytes = decoded.decoded;
        if (output != nullptr)
        {
            *output = bytes;
        }
        return true;
    }

    bool DataConvertService::parseDecimalBytes(const QString &input,
                                               QByteArray *output,
                                               QString *errorMessage)
    {
        const QString trimmed = input.trimmed();
        if (trimmed.isEmpty())
        {
            if (output != nullptr)
            {
                output->clear();
            }
            return true;
        }

        static const QRegularExpression kListPattern(QStringLiteral("^\\d+(?:[\\s,;]+\\d+)*$"));
        if (!kListPattern.match(trimmed).hasMatch())
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("十进制字节列表只能使用空格、逗号、分号或换行分隔。");
            }
            return false;
        }

        // Add a negative sign check before the main parse loop
        if (trimmed.contains(QLatin1Char('-'))) {
            if (errorMessage != nullptr)
                *errorMessage = QStringLiteral("十进制字节值不能为负数。");
            return false;
        }

        static const QRegularExpression kTokenPattern(QStringLiteral("\\d+"));
        QByteArray bytes;
        auto iterator = kTokenPattern.globalMatch(trimmed);
        while (iterator.hasNext())
        {
            const QRegularExpressionMatch match = iterator.next();
            bool ok = false;
            const uint value = match.captured().toUInt(&ok, 10);
            if (!ok || value > 255)
            {
                if (errorMessage != nullptr)
                {
                    *errorMessage = QStringLiteral("十进制字节必须在 0 到 255 之间。");
                }
                return false;
            }
            bytes.append(static_cast<char>(value));
        }

        if (output != nullptr)
        {
            *output = bytes;
        }
        return true;
    }

    QString DataConvertService::formatHex(const QByteArray &bytes,
                                          const QString &separatorMode,
                                          bool upperCaseHex)
    {
        return ByteFormatService::formatHex(bytes, separatorMode, upperCaseHex);
    }

    QString DataConvertService::formatBinary(const QByteArray &bytes)
    {
        return ByteFormatService::formatBinary(bytes);
    }

    QString DataConvertService::formatBase64(const QByteArray &bytes)
    {
        return QString::fromLatin1(bytes.toBase64());
    }

    QString DataConvertService::formatDecimalBytes(const QByteArray &bytes)
    {
        QStringList values;
        values.reserve(bytes.size());
        for (uchar byte : bytes)
        {
            values.append(QString::number(byte));
        }
        return values.join(QLatin1Char(' '));
    }

    QByteArray DataConvertService::encodeString(const QString &text,
                                                TextEncoding encoding,
                                                bool *ok,
                                                QString *errorMessage)
    {
        if (ok != nullptr)
        {
            *ok = true;
        }

        switch (encoding)
        {
        case TextEncoding::ASCII:
        {
            QByteArray bytes;
            bytes.reserve(text.size());
            for (QChar character : text)
            {
                if (character.unicode() > 0x7F)
                {
                    if (ok != nullptr)
                    {
                        *ok = false;
                    }
                    if (errorMessage != nullptr)
                    {
                        *errorMessage = QStringLiteral("ASCII 编码不支持非英文字符。");
                    }
                    return {};
                }
                bytes.append(static_cast<char>(character.unicode()));
            }
            return bytes;
        }
        case TextEncoding::UTF8:
            return text.toUtf8();
        case TextEncoding::GBK:
            return encodeGbk(text, ok, errorMessage);
        case TextEncoding::UTF16LE:
            return encodeUtf16(text, false);
        case TextEncoding::UTF16BE:
            return encodeUtf16(text, true);
        }

        return {};
    }

    QString DataConvertService::decodeBytesToString(const QByteArray &bytes,
                                                    TextEncoding encoding,
                                                    bool *ok,
                                                    QString *errorMessage)
    {
        if (ok != nullptr)
        {
            *ok = true;
        }

        switch (encoding)
        {
        case TextEncoding::ASCII:
        {
            for (uchar byte : bytes)
            {
                if (byte > 0x7F)
                {
                    if (ok != nullptr)
                    {
                        *ok = false;
                    }
                    if (errorMessage != nullptr)
                    {
                        *errorMessage = QStringLiteral("ASCII 解码失败，检测到非 ASCII 字节。");
                    }
                    return {};
                }
            }
            return QString::fromLatin1(bytes);
        }
        case TextEncoding::UTF8:
        {
            const QString text = QString::fromUtf8(bytes.constData(), bytes.size());
            return text;
        }
        case TextEncoding::GBK:
            return decodeGbk(bytes, ok, errorMessage);
        case TextEncoding::UTF16LE:
            return decodeUtf16(bytes, false, ok, errorMessage);
        case TextEncoding::UTF16BE:
            return decodeUtf16(bytes, true, ok, errorMessage);
        }

        return {};
    }

    DataConvertService::DataFormat DataConvertService::dataFormatFromKey(const QString &key)
    {
        if (key == QStringLiteral("hex"))
        {
            return DataFormat::Hex;
        }
        if (key == QStringLiteral("binary"))
        {
            return DataFormat::Binary;
        }
        if (key == QStringLiteral("base64"))
        {
            return DataFormat::Base64;
        }
        if (key == QStringLiteral("decimal"))
        {
            return DataFormat::DecimalBytes;
        }
        return DataFormat::String;
    }

    DataConvertService::TextEncoding DataConvertService::textEncodingFromKey(const QString &key)
    {
        if (key == QStringLiteral("ascii"))
        {
            return TextEncoding::ASCII;
        }
        if (key == QStringLiteral("gbk"))
        {
            return TextEncoding::GBK;
        }
        if (key == QStringLiteral("utf16le"))
        {
            return TextEncoding::UTF16LE;
        }
        if (key == QStringLiteral("utf16be"))
        {
            return TextEncoding::UTF16BE;
        }
        return TextEncoding::UTF8;
    }

} // namespace est
