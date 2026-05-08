#include "FileCompareEngine.h"

#include <QTest>

using namespace est;

class FileCompareEngineTests : public QObject
{
    Q_OBJECT

private slots:
    void pairsSingleLineEditsAsModified()
    {
        const QList<DiffLine> diffs = FileCompareEngine::compare(
            {QStringLiteral("one"), QStringLiteral("old value"), QStringLiteral("three")},
            {QStringLiteral("one"), QStringLiteral("new value"), QStringLiteral("three")});

        QCOMPARE(diffs.size(), 3);
        QCOMPARE(diffs.at(1).status, DiffLine::Modified);
        QCOMPARE(diffs.at(1).textA, QStringLiteral("old value"));
        QCOMPARE(diffs.at(1).textB, QStringLiteral("new value"));
    }

    void supportsComparisonOptionsForWhitespaceAndCase()
    {
        FileCompareOptions options;
        options.ignoreWhitespace = true;
        options.ignoreCase = true;

        const QList<DiffLine> diffs = FileCompareEngine::compare(
            {QStringLiteral("BaudRate = 115200")},
            {QStringLiteral("baudrate=115200")},
            options);

        QCOMPARE(diffs.size(), 1);
        QCOMPARE(diffs.at(0).status, DiffLine::Equal);
        QCOMPARE(diffs.at(0).textA, QStringLiteral("BaudRate = 115200"));
        QCOMPARE(diffs.at(0).textB, QStringLiteral("baudrate=115200"));
    }

    void largeInputsUsePositionFallbackInsteadOfAllRemovedThenAllAdded()
    {
        QStringList left;
        QStringList right;
        for (int i = 0; i < 5200; ++i)
        {
            left.append(QStringLiteral("line %1").arg(i));
            right.append(QStringLiteral("line %1").arg(i));
        }
        right[1234] = QStringLiteral("line changed");

        const QList<DiffLine> diffs = FileCompareEngine::compare(left, right);

        QCOMPARE(diffs.size(), 5200);
        QCOMPARE(diffs.at(1234).status, DiffLine::Modified);
        QCOMPARE(diffs.at(1233).status, DiffLine::Equal);
        QCOMPARE(diffs.at(1235).status, DiffLine::Equal);
    }

    void largeInputsKeepSingleInsertedLineAnchored()
    {
        QStringList left;
        QStringList right;
        for (int i = 0; i < 5200; ++i)
        {
            const QString line = QStringLiteral("line %1").arg(i);
            left.append(line);
            right.append(line);
        }

        right.insert(1234, QStringLiteral("inserted line"));

        const QList<DiffLine> diffs = FileCompareEngine::compare(left, right);

        QCOMPARE(diffs.size(), 5201);
        QCOMPARE(diffs.at(1233).status, DiffLine::Equal);
        QCOMPARE(diffs.at(1234).status, DiffLine::Added);
        QCOMPARE(diffs.at(1234).textA, QString());
        QCOMPARE(diffs.at(1234).textB, QStringLiteral("inserted line"));
        QCOMPARE(diffs.at(1235).status, DiffLine::Equal);
        QCOMPARE(diffs.at(1235).textA, QStringLiteral("line 1234"));
        QCOMPARE(diffs.at(1235).textB, QStringLiteral("line 1234"));
    }
};

QTEST_MAIN(FileCompareEngineTests)
#include "file_compare_engine_tests.moc"
