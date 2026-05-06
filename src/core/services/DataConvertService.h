#ifndef EST_DATACONVERTSERVICE_H
#define EST_DATACONVERTSERVICE_H

#include <QByteArray>
#include <QString>

namespace est
{

    class DataConvertService
    {
    public:
        enum class DataFormat
        {
            String,
            Hex,
            Binary,
            Base64,
            DecimalBytes
        };

        enum class TextEncoding
        {
            ASCII,
            UTF8,
            GBK,
            UTF16LE,
            UTF16BE
        };

        struct ConversionOptions
        {
            DataFormat inputFormat = DataFormat::String;
            DataFormat outputFormat = DataFormat::Hex;
            TextEncoding encoding = TextEncoding::UTF8;
            QString hexSeparatorMode = QStringLiteral("space");
            bool upperCaseHex = true;
        };

        static bool convert(const QString &input,
                            const ConversionOptions &options,
                            QString *output,
                            QString *errorMessage);

        static bool decodeToBytes(const QString &input,
                                  DataFormat format,
                                  TextEncoding encoding,
                                  QByteArray *output,
                                  QString *errorMessage);

        static QString encodeBytes(const QByteArray &bytes,
                                   DataFormat format,
                                   TextEncoding encoding,
                                   const QString &hexSeparatorMode,
                                   bool upperCaseHex,
                                   QString *errorMessage);

        static bool parseHex(const QString &input,
                             QByteArray *output,
                             QString *errorMessage);

        static bool parseBinary(const QString &input,
                                QByteArray *output,
                                QString *errorMessage);

        static bool parseBase64(const QString &input,
                                QByteArray *output,
                                QString *errorMessage);

        static bool parseDecimalBytes(const QString &input,
                                      QByteArray *output,
                                      QString *errorMessage);

        static QString formatHex(const QByteArray &bytes,
                                 const QString &separatorMode,
                                 bool upperCaseHex);

        static QString formatBinary(const QByteArray &bytes);

        static QString formatBase64(const QByteArray &bytes);

        static QString formatDecimalBytes(const QByteArray &bytes);

        static QByteArray encodeString(const QString &text,
                                       TextEncoding encoding,
                                       bool *ok = nullptr,
                                       QString *errorMessage = nullptr);

        static QString decodeBytesToString(const QByteArray &bytes,
                                           TextEncoding encoding,
                                           bool *ok = nullptr,
                                           QString *errorMessage = nullptr);

        static DataFormat dataFormatFromKey(const QString &key);
        static TextEncoding textEncodingFromKey(const QString &key);
    };

} // namespace est

#endif // EST_DATACONVERTSERVICE_H
