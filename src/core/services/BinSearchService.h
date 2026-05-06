#ifndef EST_BINSEARCHSERVICE_H
#define EST_BINSEARCHSERVICE_H

#include "services/ByteFormatService.h"

#include <QVector>

namespace est
{

    class BinSearchService
    {
    public:
        enum class SearchType
        {
            String,
            Hex,
            Offset
        };

        struct SearchQuery
        {
            SearchType type = SearchType::String;
            QString input;
            ByteFormatService::TextEncoding encoding = ByteFormatService::TextEncoding::UTF8;
        };

        static bool search(const QByteArray &data,
                           const SearchQuery &query,
                           QVector<qsizetype> *results,
                           QString *errorMessage);

        static bool parseOffset(const QString &input,
                                qsizetype *offset,
                                QString *errorMessage);
    };

} // namespace est

#endif // EST_BINSEARCHSERVICE_H
