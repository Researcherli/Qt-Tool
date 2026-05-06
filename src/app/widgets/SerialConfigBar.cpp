#include "widgets/SerialConfigBar.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace est
{
    namespace
    {

        class DownwardPopupComboBox : public QComboBox
        {
        public:
            explicit DownwardPopupComboBox(QWidget *parent = nullptr)
                : QComboBox(parent)
            {
                setProperty("popupPlacement", QStringLiteral("below"));
            }

            void showPopup() override
            {
                // Trigger refresh by sending custom event or casting parent
                if (auto* bar = qobject_cast<SerialConfigBar*>(parent())) {
                    emit bar->refreshRequested();
                }
                QComboBox::showPopup();

                QWidget *popupWindow = view() != nullptr ? view()->window() : nullptr;
                if (popupWindow == nullptr)
                {
                    return;
                }

                const QPoint popupTopLeft = mapToGlobal(QPoint(0, height()));
                popupWindow->move(popupTopLeft);
                if (popupWindow->width() < width())
                {
                    popupWindow->resize(width(), popupWindow->height());
                }
            }

        };

    } // namespace

    SerialConfigBar::SerialConfigBar(QWidget *parent)
        : QWidget(parent)
    {
        auto *rootLayout = new QHBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(8);
        setObjectName(QStringLiteral("serialConfigBar"));

        m_portCombo = new DownwardPopupComboBox(this);
        m_portCombo->setObjectName(QStringLiteral("serialPortCombo"));
        m_portCombo->setMinimumWidth(360);
        m_portCombo->setMinimumHeight(44);

        // Increase font size for combo box text
        QFont portFont = m_portCombo->font();
        portFont.setPointSize(portFont.pointSize() > 0 ? portFont.pointSize() + 2 : 12);
        m_portCombo->setFont(portFont);

        // Make list view items bigger
        // Note: do NOT pass m_portCombo as parent here; setView() takes ownership,
        // so giving the view a parent would cause a double-free when the combo is destroyed.
        auto *portView = new QListView();
        portView->setSpacing(4);
        portView->setFont(portFont);
        m_portCombo->setView(portView);
        m_baudRateCombo = new QComboBox(this);
        m_baudRateCombo->setMinimumWidth(86);
        m_dataBitsCombo = new QComboBox(this);
        m_stopBitsCombo = new QComboBox(this);
        m_parityCombo = new QComboBox(this);
        m_refreshButton = new QPushButton(tr("刷新"), this);
        m_settingsButton = new QPushButton(tr("设置"), this);
        m_openButton = new QPushButton(tr("打开串口"), this);
        m_openButton->setObjectName(QStringLiteral("openSerialButton"));
        m_closeButton = new QPushButton(tr("关闭串口"), this);
        m_closeButton->setObjectName(QStringLiteral("closeSerialButton"));

        m_baudRateCombo->addItems({QStringLiteral("9600"), QStringLiteral("19200"), QStringLiteral("38400"), QStringLiteral("57600"), QStringLiteral("115200"), QStringLiteral("460800"), QStringLiteral("921600")});
        m_dataBitsCombo->addItems({QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7"), QStringLiteral("8")});
        m_stopBitsCombo->addItem(QStringLiteral("1"), QStringLiteral("1"));
        m_stopBitsCombo->addItem(QStringLiteral("1.5"), QStringLiteral("1.5"));
        m_stopBitsCombo->addItem(QStringLiteral("2"), QStringLiteral("2"));
        m_parityCombo->addItem(tr("无校验"), QStringLiteral("none"));
        m_parityCombo->addItem(tr("奇校验"), QStringLiteral("odd"));
        m_parityCombo->addItem(tr("偶校验"), QStringLiteral("even"));

        // Hide advanced configs to keep it clean like the reference image
        m_dataBitsCombo->hide();
        m_stopBitsCombo->hide();
        m_parityCombo->hide();

        rootLayout->addWidget(m_portCombo, 1);
        rootLayout->addWidget(m_refreshButton);
        rootLayout->addWidget(m_baudRateCombo);
        rootLayout->addWidget(m_openButton);
        rootLayout->addWidget(m_closeButton);
        m_settingsButton->hide();

        // We use hidden labels/indicators so other logic still works
        m_statusIndicator = new QLabel(this);
        m_statusIndicator->hide();
        m_statusLabel = new QLabel(this);
        m_statusLabel->hide();

        m_closeButton->hide(); // Only show open initially

        connect(m_refreshButton, &QPushButton::clicked, this, &SerialConfigBar::refreshRequested);
        connect(m_settingsButton, &QPushButton::clicked, this, &SerialConfigBar::settingsRequested);
        connect(m_openButton, &QPushButton::clicked, this, &SerialConfigBar::openRequested);
        connect(m_closeButton, &QPushButton::clicked, this, &SerialConfigBar::closeRequested);
        connect(m_portCombo, &QComboBox::currentTextChanged, this, [this](const QString &) { emitSettingsChanged(); });
        connect(m_baudRateCombo, &QComboBox::currentTextChanged, this, [this](const QString &) { emitSettingsChanged(); });
        connect(m_dataBitsCombo, &QComboBox::currentTextChanged, this, [this](const QString &) { emitSettingsChanged(); });
        connect(m_stopBitsCombo, &QComboBox::currentTextChanged, this, [this](const QString &) { emitSettingsChanged(); });
        connect(m_parityCombo, &QComboBox::currentTextChanged, this, [this](const QString &) { emitSettingsChanged(); });

        setConnectionState(false, tr("未连接"), QStringLiteral("disconnected"));
    }

    void SerialConfigBar::setPortItems(const QList<QPair<QString, QString>> &ports, const QString &preferredPort)
    {
        const QString currentPort = preferredPort.isEmpty() ? currentPortName() : preferredPort;
        QSignalBlocker blocker(m_portCombo);
        m_portCombo->clear();
        int selectedIndex = 0;
        for (int index = 0; index < ports.size(); ++index)
        {
            m_portCombo->addItem(ports.at(index).first, ports.at(index).second);
            if (!currentPort.isEmpty() && ports.at(index).second == currentPort)
            {
                selectedIndex = index;
            }
        }
        if (ports.isEmpty())
        {
            m_portCombo->addItem(tr("未检测到串口"), QString());
        }
        else
        {
            m_portCombo->setCurrentIndex(selectedIndex);
        }
    }

    QVariantMap SerialConfigBar::currentConfig() const
    {
        QVariantMap config;
        config.insert(QStringLiteral("portName"), currentPortName());
        config.insert(QStringLiteral("baudRate"), m_baudRateCombo->currentText().toInt());
        config.insert(QStringLiteral("dataBits"), m_dataBitsCombo->currentText().toInt());
        config.insert(QStringLiteral("stopBits"), m_stopBitsCombo->currentData().toString());
        config.insert(QStringLiteral("parity"), m_parityCombo->currentData().toString());
        config.insert(QStringLiteral("flowControl"), QStringLiteral("none"));
        return config;
    }

    QVariantMap SerialConfigBar::savedSettings() const
    {
        QVariantMap settings = currentConfig();
        return settings;
    }

    void SerialConfigBar::applySettings(const QVariantMap &settings)
    {
        const int baudIndex = m_baudRateCombo->findText(settings.value(QStringLiteral("baudRate"), QStringLiteral("115200")).toString());
        if (baudIndex >= 0)
        {
            m_baudRateCombo->setCurrentIndex(baudIndex);
        }
        const int dataBitsIndex = m_dataBitsCombo->findText(settings.value(QStringLiteral("dataBits"), QStringLiteral("8")).toString());
        if (dataBitsIndex >= 0)
        {
            m_dataBitsCombo->setCurrentIndex(dataBitsIndex);
        }
        const int stopBitsIndex = m_stopBitsCombo->findData(settings.value(QStringLiteral("stopBits"), QStringLiteral("1")).toString());
        if (stopBitsIndex >= 0)
        {
            m_stopBitsCombo->setCurrentIndex(stopBitsIndex);
        }
        const int parityIndex = m_parityCombo->findData(settings.value(QStringLiteral("parity"), QStringLiteral("none")).toString());
        if (parityIndex >= 0)
        {
            m_parityCombo->setCurrentIndex(parityIndex);
        }
    }

    bool SerialConfigBar::showAdvancedSettingsDialog()
    {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("串口高级设置"));

        auto *layout = new QVBoxLayout(&dialog);
        auto *formLayout = new QFormLayout();

        auto *dataBitsCombo = new QComboBox(&dialog);
        dataBitsCombo->setObjectName(QStringLiteral("advancedDataBitsCombo"));
        dataBitsCombo->addItems({QStringLiteral("5"), QStringLiteral("6"), QStringLiteral("7"), QStringLiteral("8")});
        dataBitsCombo->setCurrentText(m_dataBitsCombo->currentText());

        auto *stopBitsCombo = new QComboBox(&dialog);
        stopBitsCombo->setObjectName(QStringLiteral("advancedStopBitsCombo"));
        stopBitsCombo->addItem(QStringLiteral("1"), QStringLiteral("1"));
        stopBitsCombo->addItem(QStringLiteral("1.5"), QStringLiteral("1.5"));
        stopBitsCombo->addItem(QStringLiteral("2"), QStringLiteral("2"));
        stopBitsCombo->setCurrentIndex(stopBitsCombo->findData(m_stopBitsCombo->currentData()));

        auto *parityCombo = new QComboBox(&dialog);
        parityCombo->setObjectName(QStringLiteral("advancedParityCombo"));
        parityCombo->addItem(tr("无校验"), QStringLiteral("none"));
        parityCombo->addItem(tr("奇校验"), QStringLiteral("odd"));
        parityCombo->addItem(tr("偶校验"), QStringLiteral("even"));
        parityCombo->setCurrentIndex(parityCombo->findData(m_parityCombo->currentData()));

        formLayout->addRow(tr("数据位"), dataBitsCombo);
        formLayout->addRow(tr("停止位"), stopBitsCombo);
        formLayout->addRow(tr("校验位"), parityCombo);
        layout->addLayout(formLayout);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);

        if (dialog.exec() != QDialog::Accepted)
        {
            return false;
        }

        m_dataBitsCombo->setCurrentText(dataBitsCombo->currentText());
        const int stopBitsIndex = m_stopBitsCombo->findData(stopBitsCombo->currentData());
        if (stopBitsIndex >= 0)
        {
            m_stopBitsCombo->setCurrentIndex(stopBitsIndex);
        }
        const int parityIndex = m_parityCombo->findData(parityCombo->currentData());
        if (parityIndex >= 0)
        {
            m_parityCombo->setCurrentIndex(parityIndex);
        }
        emitSettingsChanged();
        return true;
    }

    QString SerialConfigBar::currentPortName() const
    {
        return m_portCombo->currentData().toString();
    }

    QString SerialConfigBar::dataFormatSummary() const
    {
        return QStringLiteral("%1-%2-%3")
            .arg(m_dataBitsCombo->currentText(),
                 m_parityCombo->currentData().toString().toUpper(),
                 m_stopBitsCombo->currentText());
    }

    void SerialConfigBar::setConnectionState(bool connected, const QString &statusText, const QString &level)
    {
        m_statusLabel->setText(statusText);
        m_statusIndicator->setProperty("status", level);
        m_statusIndicator->style()->unpolish(m_statusIndicator);
        m_statusIndicator->style()->polish(m_statusIndicator);

        m_refreshButton->setEnabled(!connected);
        m_settingsButton->setEnabled(!connected);
        m_portCombo->setEnabled(!connected);
        m_baudRateCombo->setEnabled(!connected);

        if (connected) {
            m_openButton->hide();
            m_closeButton->show();
        } else {
            m_closeButton->hide();
            m_openButton->show();
        }
    }

    void SerialConfigBar::emitSettingsChanged()
    {
        emit settingsChanged();
    }

} // namespace est
