#ifndef EST_STRINGEXTRACTPANEL_H
#define EST_STRINGEXTRACTPANEL_H

#include "services/StringExtractor.h"

#include <QWidget>

class QComboBox;
class QLineEdit;
class QSpinBox;
class QTableWidget;

namespace est
{

    class StringExtractPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit StringExtractPanel(QWidget *parent = nullptr);

        void setData(const QByteArray &data);
        void clear();
        void exportResults();

    signals:
        void offsetActivated(qint64 offset, int length);
        void statusMessageGenerated(const QString &text);

    private:
        void refreshResults();
        void applyFilter();

        QByteArray m_data;
        QList<ExtractedStringEntry> m_entries;
        QSpinBox *m_minLengthSpinBox = nullptr;
        QLineEdit *m_filterEdit = nullptr;
        QTableWidget *m_tableWidget = nullptr;
    };

} // namespace est

#endif // EST_STRINGEXTRACTPANEL_H
