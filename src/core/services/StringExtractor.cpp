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
                return isAsciiPrintable(byte);

            if (byte == 0x00)
                return false;

            if (encoding == ByteFormatService::TextEncoding::UTF8) {
                // In UTF-8, 0x80-0x9F are continuation or invalid start bytes
                // 0xC0-0xDF are 2-byte start, 0xE0-0xEF 3-byte, 0xF0-0xF4 4-byte
                if (byte >= 0x20 && byte <= 0x7E)
                    return true;  // ASCII printable
                if (byte >= 0xA0)
                    return true;  // Non-ASCII UTF-8 start or continuation
                return false;     // 0x80-0x9F, 0x7F are invalid in our context
            }

            // For GBK and other encodings
            if (byte >= 0x20 && byte != 0x7F)
                return true;

            return byte >= 0x80;
        }

        bool looksMeaningful(const QString &text, int minimumLength)
        {
            const QString trimmed = text.trimmed();
            if (trimmed.size() < minimumLength)
            {
                return false;
            }

            // Intentional: filtering pure-punctuation to reduce false positives.
            // If you need all extractable strings, lower minimumLength instead.
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
        // Single-pass scan: does not detect overlapping strings from different encodings.
        // For overlapping detection, run extract() separately per encoding and merge results.
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
