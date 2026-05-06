#ifndef EST_STRINGEXTRACTOR_H
#define EST_STRINGEXTRACTOR_H

#include "services/ByteFormatService.h"

#include <QList>
#include <QString>

namespace est
{

    struct ExtractedStringEntry
    {
        qsizetype offset = 0;
        int length = 0;
        QString encoding;
        QString content;
    };

    class StringExtractor
    {
    public:
        static QList<ExtractedStringEntry> extract(const QByteArray &data,
                                                   int minimumLength,
                                                   ByteFormatService::TextEncoding encoding);
    };

} // namespace est

#endif // EST_STRINGEXTRACTOR_H
