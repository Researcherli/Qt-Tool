#include "pages/FileComparePage.h"

#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTest>

using namespace est;

namespace
{
    QPushButton *findButtonByText(QWidget *parent, const QString &text)
    {
        for (QPushButton *button : parent->findChildren<QPushButton *>())
        {
            if (button->text() == text)
            {
                return button;
            }
        }
        return nullptr;
    }
}

class FileComparePageTests : public QObject
{
    Q_OBJECT

private slots:
    void startsInEmptyStateWithCompareActionsDisabled()
    {
        FileComparePage page;

        auto *contentStack = page.findChild<QStackedWidget *>(QStringLiteral("compareContentStack"));
        QVERIFY(contentStack != nullptr);
        QCOMPARE(contentStack->currentIndex(), 0);

        auto *compareButton = findButtonByText(&page, QStringLiteral("比较"));
        auto *nextDiffButton = findButtonByText(&page, QStringLiteral("下一处"));
        QVERIFY(compareButton != nullptr);
        QVERIFY(nextDiffButton != nullptr);
        QVERIFY(!compareButton->isEnabled());
        QVERIFY(!nextDiffButton->isEnabled());
    }

    void exposesSummaryAndLegendWidgets()
    {
        FileComparePage page;

        auto *summaryLabel = page.findChild<QLabel *>(QStringLiteral("compareSummaryLabel"));
        QVERIFY(summaryLabel != nullptr);
        QCOMPARE(summaryLabel->text(), QStringLiteral("请选择两个文件进行比较"));

        QVERIFY(page.findChild<QLabel *>(QStringLiteral("compareLegendAdded")) != nullptr);
        QVERIFY(page.findChild<QLabel *>(QStringLiteral("compareLegendRemoved")) != nullptr);
        QVERIFY(page.findChild<QLabel *>(QStringLiteral("compareLegendModified")) != nullptr);
    }
};

QTEST_MAIN(FileComparePageTests)
#include "file_compare_page_tests.moc"
