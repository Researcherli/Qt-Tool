#pragma once

#include <QStringList>
#include <QList>
#include <QVector>
#include <QByteArray>

namespace est {

struct DiffLine {
    QString textA;
    QString textB;
    enum Status { Equal, Added, Removed, Modified };
    Status status;

    // 字节级差异（仅二进制 Modified 行使用）
    struct ByteDiff {
        int offset = 0;   // 在 hex dump 字符串中的字符偏移
        int length = 0;   // 变化的字符数
    };
    QVector<ByteDiff> byteDiffs;
};

// 编码检测工具
namespace encoding {
    QString detectEncoding(const QByteArray &data);
    QStringList supportedEncodings();
}

struct FileCompareOptions {
    bool ignoreWhitespace = false;
    bool ignoreCase = false;
    int maxExactLines = 5000;
};

class FileCompareEngine {
public:
    static QList<DiffLine> compare(const QStringList& linesA,
                                   const QStringList& linesB,
                                   const FileCompareOptions& options = {});

    // 导出功能
    static QString exportUnifiedDiff(const QStringList &linesA, const QStringList &linesB,
                                     const QString &fileA, const QString &fileB,
                                     const QList<DiffLine> &diffs);
    static QString exportHtmlDiff(const QList<DiffLine> &diffs,
                                  const QString &leftTitle, const QString &rightTitle);
    static QString exportTextSummary(const QList<DiffLine> &diffs);

};

} // namespace est
