#include "services/BinSearchService.h"

#include <QRegularExpression>

namespace est
{

    bool BinSearchService::search(const QByteArray &data,
                                  const SearchQuery &query,
                                  QVector<qsizetype> *results,
                                  QString *errorMessage)
    {
        if (results != nullptr)
        {
            results->clear();
        }

        if (query.input.trimmed().isEmpty())
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("请输入搜索内容。");
            }
            return false;
        }

        // Currently uses QByteArray::indexOf() which internally uses Boyer-Moore.
        // For very large files (>100MB) with frequent searches, consider KMP or aho-corasick.

        if (query.type == SearchType::Offset)
        {
            qsizetype offset = 0;
            if (!parseOffset(query.input, &offset, errorMessage))
            {
                return false;
            }
            if (offset < 0 || offset >= data.size())
            {
                if (errorMessage != nullptr)
                {
                    *errorMessage = QStringLiteral("偏移超出当前文件范围。");
                }
                return false;
            }
            if (results != nullptr)
            {
                results->append(offset);
            }
            return true;
        }

        QByteArray pattern;
        if (query.type == SearchType::Hex)
        {
            if (!ByteFormatService::parseHex(query.input, &pattern, errorMessage))
            {
                return false;
            }
        }
        else
        {
            bool ok = false;
            pattern = ByteFormatService::encodeString(query.input, query.encoding, &ok, errorMessage);
            if (!ok)
            {
                return false;
            }
        }

        if (pattern.isEmpty())
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("搜索模式为空。\n请检查输入。");
            }
            return false;
        }

        for (qsizetype position = data.indexOf(pattern, 0);
             position >= 0;
             position = data.indexOf(pattern, position + 1))
        {
            if (results != nullptr)
            {
                results->append(position);
            }
        }

        return true;
    }

    bool BinSearchService::parseOffset(const QString &input,
                                       qsizetype *offset,
                                       QString *errorMessage)
    {
        bool ok = false;
        QString normalized = input.trimmed();
        int base = 10;
        if (normalized.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
        {
            normalized = normalized.mid(2);
            base = 16;
        }
        else if (normalized.contains(QRegularExpression(QStringLiteral("[A-Fa-f]"))))
        {
            base = 16;
        }

        const qlonglong value = normalized.toLongLong(&ok, base);
        if (!ok || value < 0)
        {
            if (errorMessage != nullptr)
            {
                *errorMessage = QStringLiteral("偏移格式无效。\n示例：0x1200 或 4608");
            }
            return false;
        }

        if (offset != nullptr)
        {
            *offset = static_cast<qsizetype>(value);
        }
        return true;
    }

} // namespace est
