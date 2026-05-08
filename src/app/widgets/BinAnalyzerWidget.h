#ifndef EST_BINANALYZERWIDGET_H
#define EST_BINANALYZERWIDGET_H

#include "services/BinFileLoader.h"
#include "services/BinSearchService.h"

#include <QVector>
#include <QWidget>

class QLineEdit;
class QLabel;
class QComboBox;
class QTableWidget;

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
        void loadFile(const QString &filePath);

    protected:
        void dragEnterEvent(QDragEnterEvent *event) override;
        void dropEvent(QDropEvent *event) override;

    signals:
        void currentFileChanged(const QString &fileText);
        void statusMessageGenerated(const QString &text);
        void recentRecordsChanged();

    private:
        void reloadFile();
        void exportHexText();
        void runSearch();
        void goToSearchResult(int step);
        void clearSearch();
        void updateDetail(qint64 offset, uchar byteValue, const QString &asciiText);
        void updateInspector(qint64 offset);
        void updateChecksum();
        int searchMatchLength(const BinSearchService::SearchQuery &query) const;
        QTableWidget *createKeyValueTable(const QString &objectName) const;
        QString fileSizeText(qint64 byteCount) const;

        BinFileLoader m_loader;
        SearchBarWidget *m_searchBar = nullptr;
        HexViewerWidget *m_hexViewer = nullptr;
        StringExtractPanel *m_stringPanel = nullptr;
        QLineEdit *m_filePathEdit = nullptr;
        QLabel *m_fileSizeLabel = nullptr;
        QLabel *m_detailLabel = nullptr;
        QComboBox *m_endianCombo = nullptr;
        QTableWidget *m_inspectorTable = nullptr;
        QTableWidget *m_checksumTable = nullptr;
        QVector<qsizetype> m_searchResults;
        int m_currentSearchIndex = -1;
        int m_searchMatchLength = 1;
        qint64 m_currentOffset = -1;
        RecentRecordManager *m_recentRecordManager = nullptr;
    };

} // namespace est

#endif // EST_BINANALYZERWIDGET_H
