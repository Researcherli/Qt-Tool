#ifndef EST_BYTEFORMATSERVICE_H
#define EST_BYTEFORMATSERVICE_H

#include <QByteArray>
#include <QString>

namespace est
{

    class ByteFormatService
    {
    public:
        enum class TextEncoding
        {
            ASCII,
            UTF8,
            GBK,
            UTF16LE,
            UTF16BE
        };

        static bool parseHex(const QString &input,
                             QByteArray *output,
                             QString *errorMessage);

        static bool parseBinary(const QString &input,
                                QByteArray *output,
                                QString *errorMessage);

        static QString formatHex(const QByteArray &bytes,
                                 const QString &separatorMode,
                                 bool upperCaseHex);

        static QString formatBinary(const QByteArray &bytes);

        static QString formatCArray(const QByteArray &bytes,
                                    const QString &arrayName = QStringLiteral("data"),
                                    int bytesPerLine = 12);

        static QByteArray encodeString(const QString &text,
                                       TextEncoding encoding,
                                       bool *ok = nullptr,
                                       QString *errorMessage = nullptr);

        static QString decodeBytesToString(const QByteArray &bytes,
                                           TextEncoding encoding,
                                           bool *ok = nullptr,
                                           QString *errorMessage = nullptr);

        static TextEncoding textEncodingFromKey(const QString &key);
        static QString textEncodingName(TextEncoding encoding);
    };

} // namespace est

#endif // EST_BYTEFORMATSERVICE_H
