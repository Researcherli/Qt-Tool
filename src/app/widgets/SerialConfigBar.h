#ifndef EST_SERIALCONFIGBAR_H
#define EST_SERIALCONFIGBAR_H

#include <QVariantMap>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

namespace est
{

    class SerialConfigBar : public QWidget
    {
        Q_OBJECT
        
    public:
        explicit SerialConfigBar(QWidget *parent = nullptr);

        void setPortItems(const QList<QPair<QString, QString>> &ports, const QString &preferredPort = QString());
        QVariantMap currentConfig() const;
        QVariantMap savedSettings() const;
        void applySettings(const QVariantMap &settings);
        bool showAdvancedSettingsDialog();
        QString currentPortName() const;
        QString dataFormatSummary() const;
        void setConnectionState(bool connected, const QString &statusText, const QString &level);

    signals:
        void refreshRequested();
        void openRequested();
        void closeRequested();
        void settingsRequested();
        void settingsChanged();

    private:
        void emitSettingsChanged();

        QLabel *m_statusIndicator = nullptr;
        QLabel *m_statusLabel = nullptr;
        QComboBox *m_portCombo = nullptr;
        QComboBox *m_baudRateCombo = nullptr;
        QComboBox *m_dataBitsCombo = nullptr;
        QComboBox *m_stopBitsCombo = nullptr;
        QComboBox *m_parityCombo = nullptr;
        QPushButton *m_refreshButton = nullptr;
        QPushButton *m_settingsButton = nullptr;
        QPushButton *m_openButton = nullptr;
        QPushButton *m_closeButton = nullptr;
    };

} // namespace est

#endif // EST_SERIALCONFIGBAR_H
