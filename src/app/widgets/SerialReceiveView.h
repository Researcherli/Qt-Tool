#ifndef EST_SERIALRECEIVEVIEW_H
#define EST_SERIALRECEIVEVIEW_H

#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QSyntaxHighlighter;

namespace est
{

    struct DataPacket;

    class SerialReceiveView : public QWidget
    {
        Q_OBJECT

    public:
        explicit SerialReceiveView(QWidget *parent = nullptr);

        void appendPacket(const DataPacket &packet);
        void clearEntries();
        bool showDisplaySettingsDialog();
        void setTimestampVisible(bool visible);
        bool timestampVisible() const;

    signals:
        void settingsRequested();
        void entriesCleared();
        void statusMessageGenerated(const QString &text);

    private:
        struct LogEntry
        {
            qint64 timestamp = 0;
            QByteArray payload;
            QString direction;
            QString portName;
        };

        QString formatEntry(const LogEntry &entry) const;
        void renderAll();
        void findNext(bool forward);
        void applyHighlightSettings();

        QVector<LogEntry> m_entries;
        QComboBox *m_modeCombo = nullptr;
        QCheckBox *m_autoScrollCheckBox = nullptr;
        QLineEdit *m_searchEdit = nullptr;
        QPlainTextEdit *m_textEdit = nullptr;
        QSyntaxHighlighter *m_fixedFieldHighlighter = nullptr;
        bool m_fixedFieldHighlightEnabled = true;
        bool m_timestampVisible = true;
    };

} // namespace est

#endif // EST_SERIALRECEIVEVIEW_H
