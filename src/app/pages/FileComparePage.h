#pragma once

#include <QWidget>
#include <QStringList>
#include <QList>
#include <QPair>
#include <QColor>
#include <QTimer>
#include <QSet>
#include <QMap>

#include "FileCompareEngine.h"

class QSplitter;
class QPushButton;
class QLabel;
class QCheckBox;
class QComboBox;
class QStackedWidget;
class QScrollBar;
class QLineEdit;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QFontComboBox;
class QSpinBox;
class QMenu;

namespace est
{

    class DiffMinimapWidget;
    class CodeEditor;
    class IndustrialStatusCard;
    class RecentRecordManager;

    class FileComparePage : public QWidget
    {
        Q_OBJECT

    public:
        // No ICore overload needed — FileComparePage does not require DataBus
        explicit FileComparePage(QWidget *parent = nullptr);

        void setRecentRecordManager(RecentRecordManager *manager);

        struct LoadedFile
        {
            QString path;
            QStringList lines;
            qint64 size = 0;
            bool binary = false;
            bool loaded = false;
            QString encoding;  // 检测到的编码名称
        };

    protected:
        void dragEnterEvent(QDragEnterEvent *event) override;
        void dragMoveEvent(QDragMoveEvent *event) override;
        void dropEvent(QDropEvent *event) override;

    private:
        struct RenderLine
        {
            DiffLine diff;
            QColor leftColor;
            QColor rightColor;
            bool placeholder = false;
        };

        void setupUi();
        void openFile(bool leftSide);
        void compareFiles();
        void renderDiffs();
        void renderNextChunk();
        void clearComparison();
        void swapFiles();
        void navigateDiff(int delta);
        void updateFileLabels();
        void updateVisibleState();
        void updateStatusCards();
        void updateActionStates();
        void syncScrollBar(QScrollBar *source, QScrollBar *target);
        QList<QPair<int, int>> collectDiffBlocks(const QList<DiffLine> &diffs) const;
        LoadedFile loadFileContent(const QString &path, QString *errorMessage) const;
        void loadFileFromPath(const QString &path, bool leftSide);
        void acceptCurrentBlock(bool fromLeft);
        void refreshRecentMenu();

        QPushButton *m_btnOpenLeft;
        QPushButton *m_btnOpenRight;
        QPushButton *m_btnCompare;
        QPushButton *m_btnSwap;
        QPushButton *m_btnClear;
        QPushButton *m_btnPrevDiff;
        QPushButton *m_btnNextDiff;
        QPushButton *m_btnExport = nullptr;
        QPushButton *m_btnRecent = nullptr;
        QPushButton *m_btnAcceptLeft = nullptr;
        QPushButton *m_btnAcceptRight = nullptr;
        QLabel *m_leftFileLabel;
        QLabel *m_leftPathLabel;
        QLabel *m_rightFileLabel;
        QLabel *m_rightPathLabel;
        QLabel *m_summaryLabel;
        QComboBox *m_modeCombo;
        QCheckBox *m_ignoreWhitespaceCheckBox;
        QCheckBox *m_ignoreCaseCheckBox;
        QCheckBox *m_showOnlyDiffsCheckBox;
        QComboBox *m_encodingCombo;  // 编码覆盖下拉框
        IndustrialStatusCard *m_leftStatusCard;
        IndustrialStatusCard *m_compareStatusCard;
        IndustrialStatusCard *m_rightStatusCard;
        QStackedWidget *m_contentStack;
        QSplitter *m_splitter;
        CodeEditor *m_leftEditor;
        CodeEditor *m_rightEditor;
        DiffMinimapWidget *m_minimap;
        QLineEdit *m_gotoLineEdit = nullptr;
        QFontComboBox *m_fontCombo = nullptr;
        QSpinBox *m_fontSizeSpin = nullptr;

        LoadedFile m_leftFile;
        LoadedFile m_rightFile;
        QList<DiffLine> m_allDiffs;
        QList<DiffLine> m_renderedDiffs;
        QList<int> m_diffBlockIndexes;
        int m_currentDiffIndex = -1;
        int m_addedCount = 0;
        int m_removedCount = 0;
        int m_modifiedCount = 0;
        int m_equalCount = 0;
        int m_diffBlockCount = 0;

        // P1.3: 分块渐进式渲染
        QTimer *m_renderTimer = nullptr;
        int m_renderChunkIndex = 0;
        QList<RenderLine> m_pendingRenderLines;
        QPair<QStringList, QStringList> m_pendingLineLists;

        // P1.4: 比例同步滚动
        bool m_syncingScroll = false;

        // P4.2: 折叠行可展开
        QSet<int> m_expandedBlocks;
        QMap<int, int> m_collapsedBlockMap; // renderLineIndex → visibleRangeIndex
        bool m_expandingFold = false;

        // P5.2: 最近比较记录
        RecentRecordManager *m_recentRecordManager = nullptr;
    };

}
