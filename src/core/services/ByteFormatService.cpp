#include "services/ByteFormatService.h"
#include "services/EncodingUtil.h"

#include <QChar>
#include <QRegularExpression>

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

    } // namespace

    bool ByteFormatService::parseHex(const QString &input, QByteArray *output, QString *errorMessage)
    {
        const QString normalized = normalizedHexInput(input);
        if (normalized.isEmpty())
        {
            if (output != nullptr)
            {
                output->clear();
            }
            return true;
        }

        static const QRegularExpression kHexPattern(QStringLiteral("^[0-9A-Fa-f]+$"));
        if (!kHexPattern.match(normalized).hasMatch())
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("HEX 输入只能包含 0-9、A-F、空格、逗号、0x 或 \\x 前缀。");
            }
            return false;
        }

        if ((normalized.size() % 2) != 0)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("HEX 位数不完整。\n示例：4A 43 58 58");
            }
            return false;
        }

        if (output != nullptr)
        {
            *output = QByteArray::fromHex(normalized.toLatin1());
        }
        return true;
    }

    bool ByteFormatService::parseBinary(const QString &input, QByteArray *output, QString *errorMessage)
    {
        QString normalized = input;
        normalized.remove(QRegularExpression(QStringLiteral("[\\s,;]+")));

        if (normalized.isEmpty())
        {
            if (output != nullptr)
            {
                output->clear();
            }
            return true;
        }

        static const QRegularExpression kBinaryPattern(QStringLiteral("^[01]+$"));
        if (!kBinaryPattern.match(normalized).hasMatch())
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("二进制输入只能包含 0、1 以及空格分隔符。");
            }
            return false;
        }

        if ((normalized.size() % 8) != 0)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("二进制位数必须是 8 的倍数。\n当前输入 %1 位。\n示例：01001010 01000011").arg(normalized.size());
            }
            return false;
        }

        QByteArray bytes;
        bytes.reserve(normalized.size() / 8);
        for (int index = 0; index < normalized.size(); index += 8)
        {
            const QString byteString = normalized.mid(index, 8);
            bytes.append(static_cast<char>(byteString.toUInt(nullptr, 2)));
        }

        if (output != nullptr)
        {
            *output = bytes;
        }
        return true;
    }

    QString ByteFormatService::formatHex(const QByteArray &bytes, const QString &separatorMode, bool upperCaseHex)
    {
        if (bytes.isEmpty())
        {
            return {};
        }

        const char *hexChars = upperCaseHex ? "0123456789ABCDEF" : "0123456789abcdef";
        QString sep = QStringLiteral(" ");
        bool prepend0x = false;

        int charsPerByte = 2;
        if (separatorMode == QStringLiteral("none"))
        {
            sep = QString();
        }
        else if (separatorMode == QStringLiteral("comma"))
        {
            sep = QStringLiteral(",");
            // "XX," = 3 chars per byte, but the last byte has no trailing comma
            // total = 2*N + (N-1)*1 = 3N - 1
            charsPerByte = 3;
        }
        else if (separatorMode == QStringLiteral("0x"))
        {
            prepend0x = true;
            // "0xXX" + " 0xXX" per subsequent byte
            charsPerByte = 5;
        }
        else
        {
            // "XX " = 3 chars per byte (default space mode)
            charsPerByte = 3;
        }

        int totalLen = bytes.size() * 2; // 2 hex chars per byte
        if (!sep.isEmpty())
        {
            // Add separator chars between bytes (N-1 separators)
            totalLen += (bytes.size() - 1) * sep.size();
        }
        if (prepend0x)
        {
            // "0x" prefix per byte = 2 chars * N
            totalLen += bytes.size() * 2;
        }

        QString result;
        result.resize(totalLen);
        QChar *out = result.data();

        for (int i = 0; i < bytes.size(); ++i)
        {
            const uchar b = static_cast<uchar>(bytes.at(i));
            if (prepend0x)
            {
                *out++ = QLatin1Char('0');
                *out++ = QLatin1Char('x');
            }
            *out++ = QLatin1Char(hexChars[b >> 4]);
            *out++ = QLatin1Char(hexChars[b & 0x0F]);

            if (i < bytes.size() - 1 && !sep.isEmpty())
            {
                for (int j = 0; j < sep.size(); ++j)
                {
                    *out++ = sep.at(j);
                }
            }
        }
        return result;
    }

    QString ByteFormatService::formatBinary(const QByteArray &bytes)
    {
        if (bytes.isEmpty())
        {
            return {};
        }

        QString result;
        result.resize(bytes.size() * 9 - 1);
        QChar *out = result.data();

        for (int i = 0; i < bytes.size(); ++i)
        {
            const uchar b = static_cast<uchar>(bytes.at(i));
            for (int bit = 7; bit >= 0; --bit)
            {
                *out++ = QLatin1Char(((b >> bit) & 1) ? '1' : '0');
            }
            if (i < bytes.size() - 1)
            {
                *out++ = QLatin1Char(' ');
            }
        }
        return result;
    }

    QString ByteFormatService::formatCArray(const QByteArray &bytes, const QString &arrayName, int bytesPerLine)
    {
        const QString name = sanitizedArrayName(arrayName);
        const int lineWidth = qMax(1, bytesPerLine);

        QString result;
        result.append(QStringLiteral("const uint8_t %1[] = {\n").arg(name));
        for (int index = 0; index < bytes.size(); ++index)
        {
            if ((index % lineWidth) == 0)
            {
                result.append(QStringLiteral("    "));
            }

            const uchar byte = static_cast<uchar>(bytes.at(index));
            result.append(QStringLiteral("0x%1").arg(byte, 2, 16, QLatin1Char('0')));

            if (index < bytes.size() - 1)
            {
                result.append(QStringLiteral(","));
                result.append(((index + 1) % lineWidth) == 0 ? QStringLiteral("\n") : QStringLiteral(" "));
            }
            else
            {
                result.append(QStringLiteral("\n"));
            }
        }
        result.append(QStringLiteral("};"));
        return result;
    }

    QByteArray ByteFormatService::encodeString(const QString &text, TextEncoding encoding, bool *ok, QString *errorMessage)
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

    QString ByteFormatService::decodeBytesToString(const QByteArray &bytes, TextEncoding encoding, bool *ok, QString *errorMessage)
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
            return QString::fromUtf8(bytes.constData(), bytes.size());
        case TextEncoding::GBK:
            return decodeGbk(bytes, ok, errorMessage);
        case TextEncoding::UTF16LE:
            return decodeUtf16(bytes, false, ok, errorMessage);
        case TextEncoding::UTF16BE:
            return decodeUtf16(bytes, true, ok, errorMessage);
        }

        return {};
    }

    ByteFormatService::TextEncoding ByteFormatService::textEncodingFromKey(const QString &key)
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

    QString ByteFormatService::textEncodingName(TextEncoding encoding)
    {
        switch (encoding)
        {
        case TextEncoding::ASCII:
            return QStringLiteral("ASCII");
        case TextEncoding::UTF8:
            return QStringLiteral("UTF-8");
        case TextEncoding::GBK:
            return QStringLiteral("GBK");
        case TextEncoding::UTF16LE:
            return QStringLiteral("UTF-16 LE");
        case TextEncoding::UTF16BE:
            return QStringLiteral("UTF-16 BE");
        }
        return QStringLiteral("UTF-8");
    }

} // namespace est
