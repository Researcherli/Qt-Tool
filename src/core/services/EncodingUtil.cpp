#include "services/EncodingUtil.h"

#include <QRegularExpression>
#include <QStringList>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace est
{

    // =========================================================================
    // GBK 编解码
    // =========================================================================

    QString EncodingUtil::decodeGbk(const QByteArray &bytes, bool *ok, QString *errorMessage)
    {
#ifdef Q_OS_WIN
        if (bytes.isEmpty())
        {
            if (ok != nullptr)
                *ok = true;
            return {};
        }

        const int wideSize = MultiByteToWideChar(936, MB_ERR_INVALID_CHARS,
                                                  bytes.constData(), bytes.size(),
                                                  nullptr, 0);
        if (wideSize <= 0)
        {
            if (ok != nullptr)
                *ok = false;
            if (errorMessage != nullptr)
                *errorMessage = QStringLiteral("GBK 解码失败。");
            return {};
        }

        std::wstring wideText(static_cast<size_t>(wideSize), L'\0');
        MultiByteToWideChar(936, MB_ERR_INVALID_CHARS,
                            bytes.constData(), bytes.size(),
                            wideText.data(), wideSize);
        if (ok != nullptr)
            *ok = true;
        return QString::fromStdWString(wideText);
#else
        if (ok != nullptr)
            *ok = true;
        Q_UNUSED(errorMessage)
        return QString::fromLocal8Bit(bytes);
#endif
    }

    QByteArray EncodingUtil::encodeGbk(const QString &text, bool *ok, QString *errorMessage)
    {
#ifdef Q_OS_WIN
        if (text.isEmpty())
        {
            if (ok != nullptr)
                *ok = true;
            return {};
        }

        const std::wstring wideText = text.toStdWString();
        const int byteCount = WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS,
                                                    wideText.c_str(),
                                                    static_cast<int>(wideText.size()),
                                                    nullptr, 0, nullptr, nullptr);
        if (byteCount <= 0)
        {
            if (ok != nullptr)
                *ok = false;
            if (errorMessage != nullptr)
                *errorMessage = QStringLiteral("GBK 编码失败。\n当前文本可能包含无法表示的字符。");
            return {};
        }

        QByteArray bytes(byteCount, Qt::Uninitialized);
        WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS,
                            wideText.c_str(), static_cast<int>(wideText.size()),
                            bytes.data(), byteCount, nullptr, nullptr);
        if (ok != nullptr)
            *ok = true;
        return bytes;
#else
        if (ok != nullptr)
            *ok = true;
        Q_UNUSED(errorMessage)
        return text.toLocal8Bit();
#endif
    }

    // =========================================================================
    // UTF-16 编解码
    // =========================================================================

    QString EncodingUtil::decodeUtf16(const QByteArray &bytes, bool bigEndian,
                                       bool *ok, QString *errorMessage)
    {
        if ((bytes.size() % 2) != 0)
        {
            if (ok != nullptr)
                *ok = false;
            if (errorMessage != nullptr)
                *errorMessage = QStringLiteral("UTF-16 数据长度必须为偶数。\n请检查输入字节。");
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
            *ok = true;
        return result;
    }

    QByteArray EncodingUtil::encodeUtf16(const QString &text, bool bigEndian)
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

    // =========================================================================
    // Hex 输入规范化
    // =========================================================================

    QString EncodingUtil::normalizedHexInput(QString input)
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

    // =========================================================================
    // C 数组名检查
    // =========================================================================

    QString EncodingUtil::sanitizedArrayName(const QString &arrayName)
    {
        static const QRegularExpression kIdentifierPattern(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
        if (kIdentifierPattern.match(arrayName).hasMatch())
            return arrayName;
        return QStringLiteral("data");
    }

    // =========================================================================
    // Hex 值格式化
    // =========================================================================

    QString EncodingUtil::hexPrefix(quint64 value, int digits)
    {
        return QStringLiteral("0x%1").arg(value, digits, 16, QLatin1Char('0'));
    }

    QString EncodingUtil::hexPrefix(quint8 value)
    {
        return hexPrefix(static_cast<quint64>(value), 2);
    }

    QString EncodingUtil::hexPrefix(quint16 value)
    {
        return hexPrefix(static_cast<quint64>(value), 4);
    }

    QString EncodingUtil::hexPrefix(quint32 value)
    {
        return hexPrefix(static_cast<quint64>(value), 8);
    }

} // namespace est
