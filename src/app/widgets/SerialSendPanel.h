#ifndef EST_SERIALSENDPANEL_H
#define EST_SERIALSENDPANEL_H

#include "widgets/QuickCommandPanel.h"

#include <QHash>
#include <QVariantMap>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace est
{

    class SerialSendPanel : public QWidget
    {
        Q_OBJECT

    public:
        explicit SerialSendPanel(QWidget *parent = nullptr);

        QByteArray buildPayload(QString *errorMessage) const;
        QuickCommandDefinition draftCommand() const;
        QVariantMap savedSettings() const;
        void applySettings(const QVariantMap &settings);
        void moveControlsTo(QWidget *container);
        void moveOptionsTo(QWidget *container);
        void setConnected(bool connected);
        void setCommandDraft(const QuickCommandDefinition &command);
        bool autoSendEnabled() const;
        int autoSendIntervalMs() const;
        bool timestampEnabled() const;

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;

    signals:
        void sendRequested();
        void clearDataRequested();
        void saveCommandRequested();
        void settingsChanged();
        void timestampDisplayChanged(bool enabled);

    private:
        void setHexDisplayEnabled(bool enabled);
        void updateByteCount();

        QHash<QWidget *, QPushButton *> m_optionCells;
        QPlainTextEdit *m_inputEdit = nullptr;
        QWidget *m_controlPanel = nullptr;
        QWidget *m_optionArea = nullptr;
        QHBoxLayout *m_controlsLayout = nullptr;
        QComboBox *m_modeCombo = nullptr;
        QPushButton *m_timestampCheckBox = nullptr;
        QPushButton *m_hexDisplayCheckBox = nullptr;
        QPushButton *m_hexSendCheckBox = nullptr;
        QPushButton *m_enterSendCheckBox = nullptr;
        QPushButton *m_appendNewLineCheckBox = nullptr;
        QComboBox *m_lineEndingCombo = nullptr;
        QPushButton *m_autoSendCheckBox = nullptr;
        QSpinBox *m_autoSendSpinBox = nullptr;
        QLabel *m_byteCountLabel = nullptr;
        bool m_updatingHexDisplay = false;
    };

} // namespace est

#endif // EST_SERIALSENDPANEL_H
