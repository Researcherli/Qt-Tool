#ifndef EST_BINANALYZERWIDGET_H
#define EST_BINANALYZERWIDGET_H

#include "services/BinFileLoader.h"

#include <QVector>
#include <QWidget>

class QLineEdit;
class QLabel;

namespace est
{

    class ICore;
    class HexViewerWidget;
    class RecentRecordManager;
    class SearchBarWidget;
    class StringExtractPanel;

    class BinAnalyzerWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit BinAnalyzerWidget(ICore *core, QWidget *parent = nullptr);

        void openFileDialog();

    signals:
        void currentFileChanged(const QString &fileText);
        void statusMessageGenerated(const QString &text);
        void recentRecordsChanged();

    private:
        void loadFile(const QString &filePath);
        void reloadFile();
        void exportHexText();
        void runSearch();
        void goToSearchResult(int step);
        void clearSearch();
        void updateDetail(qint64 offset, uchar byteValue, const QString &asciiText);
        QString fileSizeText(qint64 byteCount) const;

        BinFileLoader m_loader;
        SearchBarWidget *m_searchBar = nullptr;
        HexViewerWidget *m_hexViewer = nullptr;
        StringExtractPanel *m_stringPanel = nullptr;
        QLineEdit *m_filePathEdit = nullptr;
        QLabel *m_fileSizeLabel = nullptr;
        QLabel *m_detailLabel = nullptr;
        QVector<qsizetype> m_searchResults;
        int m_currentSearchIndex = -1;
        RecentRecordManager *m_recentRecordManager = nullptr;
    };

} // namespace est

#endif // EST_BINANALYZERWIDGET_H
