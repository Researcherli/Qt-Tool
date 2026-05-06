#ifndef EST_SEARCHBARWIDGET_H
#define EST_SEARCHBARWIDGET_H

#include "services/BinSearchService.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;

namespace est
{

    class SearchBarWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit SearchBarWidget(QWidget *parent = nullptr);

        BinSearchService::SearchQuery currentQuery() const;
        bool highlightEnabled() const;
        void setResultCount(int count);
        void clearInput();

    signals:
        void searchRequested();
        void previousRequested();
        void nextRequested();
        void clearRequested();

    private:
        QComboBox *m_typeCombo = nullptr;
        QLineEdit *m_inputEdit = nullptr;
        QComboBox *m_encodingCombo = nullptr;
        QLabel *m_resultLabel = nullptr;
        QCheckBox *m_highlightCheckBox = nullptr;
    };

} // namespace est

#endif // EST_SEARCHBARWIDGET_H
