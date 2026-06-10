#include "FileCompareEngine.h"
#include <QRegularExpression>
#include <QTextStream>
#include <QObject>
#include <vector>
#include <algorithm>

namespace est
{

    namespace encoding
    {

        QString detectEncoding(const QByteArray &data)
        {
            if (data.isEmpty()) return QStringLiteral("UTF-8");

            // 1. BOM 检测
            if (data.size() >= 3 && static_cast<unsigned char>(data[0]) == 0xEF
                && static_cast<unsigned char>(data[1]) == 0xBB
                && static_cast<unsigned char>(data[2]) == 0xBF) {
                return QStringLiteral("UTF-8");
            }
            if (data.size() >= 2 && static_cast<unsigned char>(data[0]) == 0xFF
                && static_cast<unsigned char>(data[1]) == 0xFE) {
                return QStringLiteral("UTF-16LE");
            }
            if (data.size() >= 2 && static_cast<unsigned char>(data[0]) == 0xFE
                && static_cast<unsigned char>(data[1]) == 0xFF) {
                return QStringLiteral("UTF-16BE");
            }

            // 2. UTF-8 合法性检查（检查前 4096 字节）
            int checkLen = std::min(static_cast<int>(data.size()), 4096);
            bool isUtf8 = true;
            int i = 0;
            while (i < checkLen) {
                unsigned char c = static_cast<unsigned char>(data[i]);
                if (c < 0x80) {
                    ++i;
                } else if ((c & 0xE0) == 0xC0) {
                    if (i + 1 >= checkLen || (static_cast<unsigned char>(data[i+1]) & 0xC0) != 0x80) { isUtf8 = false; break; }
                    i += 2;
                } else if ((c & 0xF0) == 0xE0) {
                    if (i + 2 >= checkLen || (static_cast<unsigned char>(data[i+1]) & 0xC0) != 0x80
                        || (static_cast<unsigned char>(data[i+2]) & 0xC0) != 0x80) { isUtf8 = false; break; }
                    i += 3;
                } else if ((c & 0xF8) == 0xF0) {
                    if (i + 3 >= checkLen || (static_cast<unsigned char>(data[i+1]) & 0xC0) != 0x80
                        || (static_cast<unsigned char>(data[i+2]) & 0xC0) != 0x80
                        || (static_cast<unsigned char>(data[i+3]) & 0xC0) != 0x80) { isUtf8 = false; break; }
                    i += 4;
                } else {
                    isUtf8 = false;
                    break;
                }
            }
            if (isUtf8) return QStringLiteral("UTF-8");

            // 3. GBK/GB2312 启发式检测（检查是否有大量双字节高位模式）
            int gbkScore = 0;
            int asciiCount = 0;
            for (int j = 0; j < checkLen; ++j) {
                unsigned char c = static_cast<unsigned char>(data[j]);
                if (c < 0x80) {
                    ++asciiCount;
                } else if (c >= 0x81 && c <= 0xFE) {
                    ++gbkScore;
                }
            }
            // 如果高位字节比例较高，可能是 GBK
            if (gbkScore > 0 && gbkScore > checkLen / 10) {
                return QStringLiteral("GBK");
            }

            // 4. 兜底：系统本地编码
            return QStringLiteral("System");
        }

        QStringList supportedEncodings()
        {
            return {QStringLiteral("UTF-8"), QStringLiteral("GBK"),
                    QStringLiteral("UTF-16LE"), QStringLiteral("UTF-16BE"),
                    QStringLiteral("System")};
        }

    } // namespace encoding

    namespace
    {
        double lineSimilarity(const QString &a, const QString &b)
        {
            const int aLen = a.size();
            const int bLen = b.size();
            const int maxLen = std::max(1, std::max(aLen, bLen));
            const int commonLen = std::min(aLen, bLen);

            int prefix = 0;
            while (prefix < commonLen && a.at(prefix) == b.at(prefix))
                ++prefix;

            int suffix = 0;
            while (suffix < commonLen - prefix && a.at(aLen - 1 - suffix) == b.at(bLen - 1 - suffix))
                ++suffix;

            return static_cast<double>(prefix + suffix) / maxLen;
        }

        QString normalizedLine(QString line, const FileCompareOptions &options)
        {
            if (options.ignoreWhitespace)
            {
                line.remove(QRegularExpression(QStringLiteral("\\s+")));
            }
            if (options.ignoreCase)
            {
                line = line.toCaseFolded();
            }
            return line;
        }

        QStringList normalizedLines(const QStringList &lines, const FileCompareOptions &options)
        {
            QStringList normalized;
            normalized.reserve(lines.size());
            for (const QString &line : lines)
            {
                normalized.append(normalizedLine(line, options));
            }
            return normalized;
        }

        void buildSmallDiff(const QStringList &linesA,
                            const QStringList &linesB,
                            const QStringList &normalizedA,
                            const QStringList &normalizedB,
                            int aStart,
                            int aEnd,
                            int bStart,
                            int bEnd,
                            QList<DiffLine> &result)
        {
            const int m = aEnd - aStart;
            const int n = bEnd - bStart;
            std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));

            for (int i = 1; i <= m; ++i)
            {
                for (int j = 1; j <= n; ++j)
                {
                    if (normalizedA.at(aStart + i - 1) == normalizedB.at(bStart + j - 1))
                    {
                        dp[i][j] = dp[i - 1][j - 1] + 1;
                    }
                    else
                    {
                        dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
                    }
                }
            }

            QList<DiffLine> local;
            int i = m;
            int j = n;
            while (i > 0 || j > 0)
            {
                if (i > 0 && j > 0 && normalizedA.at(aStart + i - 1) == normalizedB.at(bStart + j - 1))
                {
                    local.prepend({linesA.at(aStart + i - 1), linesB.at(bStart + j - 1), DiffLine::Equal});
                    --i;
                    --j;
                }
                else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j]))
                {
                    local.prepend({QString(), linesB.at(bStart + j - 1), DiffLine::Added});
                    --j;
                }
                else
                {
                    local.prepend({linesA.at(aStart + i - 1), QString(), DiffLine::Removed});
                    --i;
                }
            }

            result.append(local);
        }

        std::vector<int> lcsPrefixScores(const QStringList &normalizedA,
                                         const QStringList &normalizedB,
                                         int aStart,
                                         int aEnd,
                                         int bStart,
                                         int bEnd)
        {
            const int bLen = bEnd - bStart;
            std::vector<int> previous(bLen + 1, 0);
            std::vector<int> current(bLen + 1, 0);

            for (int i = aStart; i < aEnd; ++i)
            {
                std::fill(current.begin(), current.end(), 0);
                for (int j = 0; j < bLen; ++j)
                {
                    if (normalizedA.at(i) == normalizedB.at(bStart + j))
                    {
                        current[j + 1] = previous[j] + 1;
                    }
                    else
                    {
                        current[j + 1] = std::max(current[j], previous[j + 1]);
                    }
                }
                previous.swap(current);
            }

            return previous;
        }

        std::vector<int> lcsSuffixScores(const QStringList &normalizedA,
                                         const QStringList &normalizedB,
                                         int aStart,
                                         int aEnd,
                                         int bStart,
                                         int bEnd)
        {
            const int bLen = bEnd - bStart;
            std::vector<int> previous(bLen + 1, 0);
            std::vector<int> current(bLen + 1, 0);

            for (int i = aEnd - 1; i >= aStart; --i)
            {
                std::fill(current.begin(), current.end(), 0);
                for (int j = bLen - 1; j >= 0; --j)
                {
                    if (normalizedA.at(i) == normalizedB.at(bStart + j))
                    {
                        current[j] = previous[j + 1] + 1;
                    }
                    else
                    {
                        current[j] = std::max(current[j + 1], previous[j]);
                    }
                }
                previous.swap(current);
            }

            return previous;
        }

        void buildLargeDiff(const QStringList &linesA,
                            const QStringList &linesB,
                            const QStringList &normalizedA,
                            const QStringList &normalizedB,
                            int aStart,
                            int aEnd,
                            int bStart,
                            int bEnd,
                            int maxExactLines,
                            std::vector<int> &scoreBuffer,
                            QList<DiffLine> &result)
        {
            while (aStart < aEnd && bStart < bEnd && normalizedA.at(aStart) == normalizedB.at(bStart))
            {
                result.append({linesA.at(aStart), linesB.at(bStart), DiffLine::Equal});
                ++aStart;
                ++bStart;
            }

            QList<DiffLine> suffix;
            while (aStart < aEnd && bStart < bEnd && normalizedA.at(aEnd - 1) == normalizedB.at(bEnd - 1))
            {
                suffix.prepend({linesA.at(aEnd - 1), linesB.at(bEnd - 1), DiffLine::Equal});
                --aEnd;
                --bEnd;
            }

            if (aStart == aEnd)
            {
                for (int index = bStart; index < bEnd; ++index)
                {
                    result.append({QString(), linesB.at(index), DiffLine::Added});
                }
                result.append(suffix);
                return;
            }

            if (bStart == bEnd)
            {
                for (int index = aStart; index < aEnd; ++index)
                {
                    result.append({linesA.at(index), QString(), DiffLine::Removed});
                }
                result.append(suffix);
                return;
            }

            const int aLen = aEnd - aStart;
            const int bLen = bEnd - bStart;
            if (aLen == 1 || bLen == 1 || static_cast<qint64>(aLen) * static_cast<qint64>(bLen) <= static_cast<qint64>(maxExactLines))
            {
                buildSmallDiff(linesA, linesB, normalizedA, normalizedB, aStart, aEnd, bStart, bEnd, result);
                result.append(suffix);
                return;
            }

            const int aMid = aStart + aLen / 2;
            scoreBuffer = lcsPrefixScores(normalizedA, normalizedB, aStart, aMid, bStart, bEnd);
            const std::vector<int> rightScores = lcsSuffixScores(normalizedA, normalizedB, aMid, aEnd, bStart, bEnd);
            const std::vector<int> &leftScores = scoreBuffer;

            int bestSplit = 0;
            int bestScore = -1;
            for (int split = 0; split <= bLen; ++split)
            {
                const int score = leftScores.at(split) + rightScores.at(split);
                if (score > bestScore)
                {
                    bestScore = score;
                    bestSplit = split;
                }
            }

            buildLargeDiff(linesA, linesB, normalizedA, normalizedB, aStart, aMid, bStart, bStart + bestSplit, maxExactLines, scoreBuffer, result);
            buildLargeDiff(linesA, linesB, normalizedA, normalizedB, aMid, aEnd, bStart + bestSplit, bEnd, maxExactLines, scoreBuffer, result);
            result.append(suffix);
        }

        QList<DiffLine> coalesceChangedRuns(const QList<DiffLine> &raw)
        {
            static constexpr int kMaxCoalesceLines = 5000;
            if (raw.size() > kMaxCoalesceLines) {
                return raw;
            }

            QList<DiffLine> result;
            int index = 0;

            while (index < raw.size())
            {
                if (raw.at(index).status == DiffLine::Equal)
                {
                    result.append(raw.at(index));
                    ++index;
                    continue;
                }

                QStringList removed;
                QStringList added;
                while (index < raw.size() && raw.at(index).status != DiffLine::Equal)
                {
                    const DiffLine &line = raw.at(index);
                    if (line.status == DiffLine::Removed)
                    {
                        removed.append(line.textA);
                    }
                    else if (line.status == DiffLine::Added)
                    {
                        added.append(line.textB);
                    }
                    ++index;
                }

                const int rSize = removed.size();
                const int aSize = added.size();

                std::vector<std::vector<double>> sim(rSize, std::vector<double>(aSize, 0.0));
                for (int i = 0; i < rSize; ++i)
                {
                    for (int j = 0; j < aSize; ++j)
                    {
                        sim[i][j] = lineSimilarity(removed.at(i), added.at(j));
                    }
                }

                std::vector<bool> rUsed(rSize, false);
                std::vector<bool> aUsed(aSize, false);

                for (int k = 0; k < std::min(rSize, aSize); ++k)
                {
                    double bestSim = 0.2;
                    int bestR = -1;
                    int bestA = -1;

                    for (int i = 0; i < rSize; ++i)
                    {
                        if (rUsed[i]) continue;
                        for (int j = 0; j < aSize; ++j)
                        {
                            if (aUsed[j]) continue;
                            if (sim[i][j] > bestSim)
                            {
                                bestSim = sim[i][j];
                                bestR = i;
                                bestA = j;
                            }
                        }
                    }

                    if (bestR < 0) break;

                    rUsed[bestR] = true;
                    aUsed[bestA] = true;
                    result.append({removed.at(bestR), added.at(bestA), DiffLine::Modified});
                }

                for (int i = 0; i < rSize; ++i)
                {
                    if (!rUsed[i])
                    {
                        result.append({removed.at(i), QString(), DiffLine::Removed});
                    }
                }
                for (int j = 0; j < aSize; ++j)
                {
                    if (!aUsed[j])
                    {
                        result.append({QString(), added.at(j), DiffLine::Added});
                    }
                }
            }

            return result;
        }

        bool isHexDumpLine(const QString &line)
        {
            if (line.size() < 11) return false;
            // 检查前 8 字符是否为十六进制
            for (int i = 0; i < 8; ++i) {
                QChar c = line.at(i);
                if (!c.isDigit() && !(c >= 'A' && c <= 'F') && !(c >= 'a' && c <= 'f'))
                    return false;
            }
            // 检查第 8-9 位置是否为 ": "
            return line.at(8) == ':' && line.at(9) == ' ';
        }

        QVector<DiffLine::ByteDiff> computeByteDiffs(const QString &hexLineA, const QString &hexLineB)
        {
            // hex dump 格式: "00000000: FF FE AB ... | .text."
            // 从偏移 10 开始比较 hex 部分（跳过地址和 ": "）
            QVector<DiffLine::ByteDiff> diffs;
            int start = -1;
            const int minLen = std::min(hexLineA.size(), hexLineB.size());
            for (int i = 10; i < minLen; ++i) {
                if (hexLineA.at(i) != hexLineB.at(i)) {
                    if (start < 0) start = i;
                } else if (start >= 0) {
                    diffs.append({start, i - start});
                    start = -1;
                }
            }
            if (start >= 0) {
                diffs.append({start, minLen - start});
            }
            return diffs;
        }
    }

    QList<DiffLine> FileCompareEngine::compare(const QStringList &linesA,
                                               const QStringList &linesB,
                                               const FileCompareOptions &options)
    {
        const QStringList normalizedA = normalizedLines(linesA, options);
        const QStringList normalizedB = normalizedLines(linesB, options);

        QList<DiffLine> rawDiffs;
        std::vector<int> scoreBuffer;
        buildLargeDiff(linesA, linesB, normalizedA, normalizedB,
                       0, linesA.size(), 0, linesB.size(), options.maxExactLines, scoreBuffer, rawDiffs);

        QList<DiffLine> result = coalesceChangedRuns(rawDiffs);

        // 为二进制 Modified 行计算字节差异
        for (auto &diff : result) {
            if (diff.status == DiffLine::Modified && isHexDumpLine(diff.textA) && isHexDumpLine(diff.textB)) {
                diff.byteDiffs = computeByteDiffs(diff.textA, diff.textB);
            }
        }

        return result;
    }

    QString FileCompareEngine::exportUnifiedDiff(const QStringList &linesA, const QStringList &linesB,
                                                  const QString &fileA, const QString &fileB,
                                                  const QList<DiffLine> &diffs)
    {
        QString result;
        QTextStream out(&result);

        out << "--- " << fileA << "\n";
        out << "+++ " << fileB << "\n";

        int i = 0;
        while (i < diffs.size()) {
            // 找到差异块
            int blockStart = i;
            while (blockStart < diffs.size() && diffs.at(blockStart).status == DiffLine::Equal) {
                ++blockStart;
            }
            if (blockStart >= diffs.size()) break;

            int blockEnd = blockStart;
            while (blockEnd < diffs.size() && diffs.at(blockEnd).status != DiffLine::Equal) {
                ++blockEnd;
            }

            // 上下文（前后各 3 行）
            int contextStart = std::max(0, blockStart - 3);
            int contextEnd = std::min(static_cast<int>(diffs.size()), blockEnd + 3);

            // 合并相邻块
            while (i < contextStart) ++i;

            // 计算行号
            int leftLine = 1;
            int rightLine = 1;
            for (int j = 0; j < contextStart; ++j) {
                if (diffs.at(j).status != DiffLine::Added) ++leftLine;
                if (diffs.at(j).status != DiffLine::Removed) ++rightLine;
            }

            int leftCount = 0, rightCount = 0;
            for (int j = contextStart; j < contextEnd; ++j) {
                if (diffs.at(j).status != DiffLine::Added) ++leftCount;
                if (diffs.at(j).status != DiffLine::Removed) ++rightCount;
            }

            out << "@@ -" << leftLine << "," << leftCount
                << " +" << rightLine << "," << rightCount << " @@\n";

            for (int j = contextStart; j < contextEnd; ++j) {
                const DiffLine &diff = diffs.at(j);
                switch (diff.status) {
                case DiffLine::Equal:
                    out << " " << diff.textA << "\n";
                    break;
                case DiffLine::Removed:
                case DiffLine::Modified:
                    out << "-" << diff.textA << "\n";
                    break;
                default: break;
                }
                switch (diff.status) {
                case DiffLine::Equal:
                    break; // already printed
                case DiffLine::Added:
                case DiffLine::Modified:
                    out << "+" << diff.textB << "\n";
                    break;
                default: break;
                }
            }

            i = contextEnd;
        }

        return result;
    }

    QString FileCompareEngine::exportHtmlDiff(const QList<DiffLine> &diffs,
                                               const QString &leftTitle, const QString &rightTitle)
    {
        QString html;
        QTextStream out(&html);

        out << "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"UTF-8\">\n";
        out << "<style>\n";
        out << "body { font-family: Consolas, monospace; font-size: 13px; background: #1e1e1e; color: #d4d4d4; margin: 20px; }\n";
        out << "table { border-collapse: collapse; width: 100%; }\n";
        out << "td { padding: 1px 8px; vertical-align: top; white-space: pre; }\n";
        out << ".ln { color: #858585; text-align: right; min-width: 40px; user-select: none; background: #252526; }\n";
        out << ".added { background: #1b3a1b; } .added .text { color: #4ec9b0; }\n";
        out << ".removed { background: #3a1b1b; } .removed .text { color: #f44747; }\n";
        out << ".modified { background: #3a3a1b; } .modified .text { color: #dcdcaa; }\n";
        out << ".sep { background: #333; height: 2px; }\n";
        out << "tr:nth-child(even) { background: #2d2d2d; }\n";
        out << "</style>\n</head>\n<body>\n";
        out << "<h2>" << leftTitle << " ↔ " << rightTitle << "</h2>\n";
        out << "<table>\n";

        for (int i = 0; i < diffs.size(); ++i) {
            const DiffLine &diff = diffs.at(i);
            QString rowClass;
            switch (diff.status) {
            case DiffLine::Added:    rowClass = "added"; break;
            case DiffLine::Removed:  rowClass = "removed"; break;
            case DiffLine::Modified: rowClass = "modified"; break;
            default: break;
            }

            out << "<tr class=\"" << rowClass << "\">";
            out << "<td class=\"ln\">" << (i + 1) << "</td>";
            if (rowClass.isEmpty()) {
                out << "<td>" << diff.textA.toHtmlEscaped() << "</td>";
                out << "<td class=\"ln\">" << (i + 1) << "</td>";
                out << "<td>" << diff.textB.toHtmlEscaped() << "</td>";
            } else {
                out << "<td class=\"text\">" << diff.textA.toHtmlEscaped() << "</td>";
                out << "<td class=\"ln\">" << (i + 1) << "</td>";
                out << "<td class=\"text\">" << diff.textB.toHtmlEscaped() << "</td>";
            }
            out << "</tr>\n";
        }

        out << "</table>\n</body>\n</html>";
        return html;
    }

    QString FileCompareEngine::exportTextSummary(const QList<DiffLine> &diffs)
    {
        int added = 0, removed = 0, modified = 0, equal = 0;
        for (const auto &diff : diffs) {
            switch (diff.status) {
            case DiffLine::Added:    ++added; break;
            case DiffLine::Removed:  ++removed; break;
            case DiffLine::Modified: ++modified; break;
            case DiffLine::Equal:    ++equal; break;
            }
        }

        QString summary;
        QTextStream out(&summary);
        out << QObject::tr("=== 文件比较摘要 ===\n");
        out << QObject::tr("总行数: %1\n").arg(diffs.size());
        out << QObject::tr("相同行: %1\n").arg(equal);
        out << QObject::tr("新增行: %1\n").arg(added);
        out << QObject::tr("删除行: %1\n").arg(removed);
        out << QObject::tr("修改行: %1\n").arg(modified);
        // Count actual diff hunks
        int hunks = 0;
        bool inHunk = false;
        for (const auto &diff : diffs) {
            if (diff.status != DiffLine::Equal) {
                if (!inHunk) {
                    ++hunks;
                    inHunk = true;
                }
            } else {
                inHunk = false;
            }
        }
        out << QObject::tr("变更块: %1\n").arg(hunks);
        return summary;
    }

} // namespace est
