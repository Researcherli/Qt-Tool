#ifndef EST_DATACONVERTWIDGET_H
#define EST_DATACONVERTWIDGET_H

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QPlainTextEdit;

namespace est
{

    class DataConvertWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit DataConvertWidget(QWidget *parent = nullptr);

    signals:
        void statusMessageGenerated(const QString &text);

    private:
        void dragEnterEvent(QDragEnterEvent *event) override;
        void dropEvent(QDropEvent *event) override;

        void performConversion();
        void updateLengthLabels();
        void clearAll();
        void swapInputOutput();

        QComboBox *m_inputTypeCombo = nullptr;
        QComboBox *m_outputTypeCombo = nullptr;
        QComboBox *m_encodingCombo = nullptr;
        QComboBox *m_separatorCombo = nullptr;
        QComboBox *m_caseCombo = nullptr;
        QCheckBox *m_autoConvertCheckBox = nullptr;
        QPlainTextEdit *m_inputEdit = nullptr;
        QPlainTextEdit *m_outputEdit = nullptr;
        QLabel *m_inputLengthLabel = nullptr;
        QLabel *m_outputLengthLabel = nullptr;
    };

} // namespace est

#endif // EST_DATACONVERTWIDGET_H
