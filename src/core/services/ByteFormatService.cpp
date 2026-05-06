#include "services/ByteFormatService.h"

#include <QChar>
#include <QRegularExpression>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace est
{

    namespace
    {

        QString decodeGbk(const QByteArray &bytes, bool *ok, QString *errorMessage)
        {
#ifdef Q_OS_WIN
            if (bytes.isEmpty())
            {
                if (ok != nullptr)
                {
                    *ok = true;
                }
                return {};
            }

            const int wideSize = MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, bytes.constData(), bytes.size(), nullptr, 0);
            if (wideSize <= 0)
            {
                if (ok != nullptr)
                {
                    *ok = false;
                }
                if (errorMessage != nullptr)
                {
                    *errorMessage = QStringLiteral("GBK 解码失败。");
                }
                return {};
            }

            std::wstring wideText(static_cast<size_t>(wideSize), L'\0');
            MultiByteToWideChar(936, MB_ERR_INVALID_CHARS, bytes.constData(), bytes.size(), wideText.data(), wideSize);
            if (ok != nullptr)
            {
                *ok = true;
            }
            return QString::fromStdWString(wideText);
#else
            if (ok != nullptr)
            {
                *ok = true;
            }
            Q_UNUSED(errorMessage)
            return QString::fromLocal8Bit(bytes);
#endif
        }

        QByteArray encodeGbk(const QString &text, bool *ok, QString *errorMessage)
        {
#ifdef Q_OS_WIN
            if (text.isEmpty())
            {
                if (ok != nullptr)
                {
                    *ok = true;
                }
                return {};
            }

            const std::wstring wideText = text.toStdWString();
            const int byteCount = WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS, wideText.c_str(), static_cast<int>(wideText.size()), nullptr, 0, nullptr, nullptr);
            if (byteCount <= 0)
            {
                if (ok != nullptr)
                {
                    *ok = false;
                }
                if (errorMessage != nullptr)
                {
                    *errorMessage = QStringLiteral("GBK 编码失败。\n当前文本可能包含无法表示的字符。");
                }
                return {};
            }

            QByteArray bytes(byteCount, Qt::Uninitialized);
            WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS, wideText.c_str(), static_cast<int>(wideText.size()), bytes.data(), byteCount, nullptr, nullptr);
            if (ok != nullptr)
            {
                *ok = true;
            }
            return bytes;
#else
            if (ok != nullptr)
            {
                *ok = true;
            }
            Q_UNUSED(errorMessage)
            return text.toLocal8Bit();
#endif
        }

        QString decodeUtf16(const QByteArray &bytes, bool bigEndian, bool *ok, QString *errorMessage)
        {
            if ((bytes.size() % 2) != 0)
            {
                if (ok != nullptr)
                {
                    *ok = false;
                }
                if (errorMessage != nullptr)
                {
                    *errorMessage = QStringLiteral("UTF-16 数据长度必须为偶数。\n请检查输入字节。");
                }
                return {};
            }

            QString result;
            result.reserve(bytes.size() / 2);
            for (int index = 0; index < bytes.size(); index += 2)
            {
                const uchar first = static_cast<uchar>(bytes.at(index));
                const uchar second = static_cast<uchar>(bytes.at(index + 1));
                const ushort codeUnit = bigEndian
                                             ? static_cast<ushort>((first << 8) | second)
                                             : static_cast<ushort>((second << 8) | first);
                result.append(QChar(codeUnit));
            }

            if (ok != nullptr)
            {
                *ok = true;
            }
            return result;
        }

        QByteArray encodeUtf16(const QString &text, bool bigEndian)
        {
            QByteArray bytes;
            bytes.reserve(text.size() * 2);
            for (QChar character : text)
            {
                const ushort codeUnit = character.unicode();
                if (bigEndian)
                {
                    bytes.append(static_cast<char>((codeUnit >> 8) & 0xFF));
                    bytes.append(static_cast<char>(codeUnit & 0xFF));
                }
                else
                {
                    bytes.append(static_cast<char>(codeUnit & 0xFF));
                    bytes.append(static_cast<char>((codeUnit >> 8) & 0xFF));
                }
            }
            return bytes;
        }

        QString normalizedHexInput(QString input)
        {
            input.replace(QRegularExpression(QStringLiteral("\\\\x"),
                                             QRegularExpression::CaseInsensitiveOption),
                          QString());
            input.replace(QRegularExpression(QStringLiteral("0x"),
                                             QRegularExpression::CaseInsensitiveOption),
                          QString());
            input.remove(QRegularExpression(QStringLiteral("[\\s,;{}\\[\\]]+")));
            return input;
        }

        QString sanitizedArrayName(const QString &arrayName)
        {
            static const QRegularExpression kIdentifierPattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
            if (kIdentifierPattern.match(arrayName).hasMatch())
            {
                return arrayName;
            }
            return QStringLiteral("data");
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
                *errorMessage = QStringLiteral("二进制位数必须是 8 的倍数。\n示例：01001010 01000011");
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
            charsPerByte = 3;
        }
        else if (separatorMode == QStringLiteral("0x"))
        {
            prepend0x = true;
            charsPerByte = 5;
        }
        else
        {
            charsPerByte = 3;
        }

        int totalLen = bytes.size() * charsPerByte;
        if (!sep.isEmpty())
        {
            totalLen -= sep.size();
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
            result.append(QStringLiteral("0x%1").arg(byte, 2, 16, QLatin1Char('0')).toUpper().replace(QStringLiteral("0X"), QStringLiteral("0x")));

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
