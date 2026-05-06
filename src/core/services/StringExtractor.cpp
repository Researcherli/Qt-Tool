#include "services/StringExtractor.h"

#include <QByteArray>

namespace est
{

    namespace
    {

        bool isAsciiPrintable(uchar byte)
        {
            return byte >= 0x20 && byte <= 0x7E;
        }

        bool isCandidateByte(uchar byte, ByteFormatService::TextEncoding encoding)
        {
            if (encoding == ByteFormatService::TextEncoding::ASCII)
            {
                return isAsciiPrintable(byte);
            }

            if (byte == 0x00)
            {
                return false;
            }

            if (byte >= 0x20 && byte != 0x7F)
            {
                return true;
            }

            return byte >= 0x80;
        }

        bool looksMeaningful(const QString &text, int minimumLength)
        {
            const QString trimmed = text.trimmed();
            if (trimmed.size() < minimumLength)
            {
                return false;
            }

            for (QChar character : trimmed)
            {
                if (character.isLetterOrNumber())
                {
                    return true;
                }
            }
            return false;
        }

    } // namespace

    QList<ExtractedStringEntry> StringExtractor::extract(const QByteArray &data,
                                                         int minimumLength,
                                                         ByteFormatService::TextEncoding encoding)
    {
        QList<ExtractedStringEntry> results;

        int startIndex = -1;
        for (int index = 0; index <= data.size(); ++index)
        {
            const bool atEnd = index >= data.size();
            const uchar byte = atEnd ? 0 : static_cast<uchar>(data.at(index));

            if (!atEnd && isCandidateByte(byte, encoding))
            {
                if (startIndex < 0)
                {
                    startIndex = index;
                }
                continue;
            }

            if (startIndex >= 0)
            {
                const QByteArray segment = data.mid(startIndex, index - startIndex);
                bool ok = false;
                QString errorMessage;
                const QString content = ByteFormatService::decodeBytesToString(segment, encoding, &ok, &errorMessage);
                Q_UNUSED(errorMessage)
                if (ok && looksMeaningful(content, minimumLength))
                {
                    ExtractedStringEntry entry;
                    entry.offset = startIndex;
                    entry.length = segment.size();
                    entry.encoding = ByteFormatService::textEncodingName(encoding);
                    entry.content = content;
                    results.append(entry);
                }
                startIndex = -1;
            }
        }

        return results;
    }

} // namespace est
