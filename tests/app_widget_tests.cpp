#include "databus/DataPacket.h"
#include "databus/DataBus.h"
#include "pages/SerialAssistantPage.h"
#include "plugin/ICore.h"
#include "widgets/QuickCommandPanel.h"
#include "widgets/DataConvertWidget.h"
#include "widgets/SerialReceiveView.h"
#include "widgets/SerialSendPanel.h"
#include "widgets/SerialConfigBar.h"
#include "services/ByteChecksumService.h"
#include "services/ByteDataInspectorService.h"
#include "services/ByteFormatService.h"
#include "services/DataConvertService.h"
#include "services/RecentRecordManager.h"
#include "transport/TransportRegistry.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QPlainTextEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTextBlock>
#include <QTextLayout>
#include <QTextStream>
#include <QTest>
#include <QTimer>
#include <QWheelEvent>

using namespace est;

namespace
{
    class FakeTransport : public ITransport
    {
    public:
        QString transportType() const override
        {
            return QStringLiteral("serial");
        }

        QString name() const override
        {
            return QStringLiteral("fake-serial");
        }

        State state() const override
        {
            return m_state;
        }

        bool open(const QVariantMap &config) override
        {
            m_name = config.value(QStringLiteral("portName")).toString();
            m_state = State::Connected;
            emit stateChanged(m_state);
            return true;
        }

        void close() override
        {
            m_state = State::Disconnected;
            emit stateChanged(m_state);
        }

        bool send(const DataPacket &packet) override
        {
            m_sentPackets.append(packet);
            return true;
        }

        QList<DataPacket> sentPackets() const
        {
            return m_sentPackets;
        }

    private:
        QString m_name;
        State m_state = State::Disconnected;
        QList<DataPacket> m_sentPackets;
    };

    class FakeCore : public ICore
    {
    public:
        FakeCore()
        {
            m_transportRegistry.registerFactory(QStringLiteral("serial"), []() {
                return new FakeTransport();
            });
        }

        DataBus *dataBus() const override
        {
            return const_cast<DataBus *>(&m_dataBus);
        }

        PluginRegistry *pluginRegistry() const override
        {
            return nullptr;
        }

        TransportRegistry *transportRegistry() const override
        {
            return const_cast<TransportRegistry *>(&m_transportRegistry);
        }

        RecentRecordManager *recentRecordManager() const override
        {
            return const_cast<RecentRecordManager *>(&m_recentRecordManager);
        }

    private:
        mutable DataBus m_dataBus;
        mutable TransportRegistry m_transportRegistry;
        mutable RecentRecordManager m_recentRecordManager;
    };

    QColor foregroundColorForToken(QPlainTextEdit *textEdit, const QString &token)
    {
        QTextBlock block = textEdit->document()->begin();
        while (block.isValid())
        {
            const int tokenIndex = block.text().indexOf(token);
            if (tokenIndex >= 0)
            {
                const QList<QTextLayout::FormatRange> ranges = block.layout()->formats();
                for (const QTextLayout::FormatRange &range : ranges)
                {
                    if (tokenIndex >= range.start && tokenIndex < range.start + range.length)
                    {
                        return range.format.foreground().color();
                    }
                }
                return QColor();
            }
            block = block.next();
        }
        return QColor();
    }

    QColor backgroundColorForToken(QPlainTextEdit *textEdit, const QString &token)
    {
        QTextBlock block = textEdit->document()->begin();
        while (block.isValid())
        {
            const int tokenIndex = block.text().indexOf(token);
            if (tokenIndex >= 0)
            {
                const QList<QTextLayout::FormatRange> ranges = block.layout()->formats();
                for (const QTextLayout::FormatRange &range : ranges)
                {
                    if (tokenIndex >= range.start && tokenIndex < range.start + range.length)
                    {
                        if (range.format.background().style() == Qt::NoBrush)
                        {
                            return QColor();
                        }
                        return range.format.background().color();
                    }
                }
                return QColor();
            }
            block = block.next();
        }
        return QColor();
    }

    QCheckBox *findCheckBoxByText(QWidget *parent, const QString &text)
    {
        for (QCheckBox *checkBox : parent->findChildren<QCheckBox *>())
        {
            if (checkBox->text() == text)
            {
                return checkBox;
            }
        }
        return nullptr;
    }

    QWidget *optionCellForToggle(QCheckBox *toggle)
    {
        QWidget *cell = toggle != nullptr ? toggle->parentWidget() : nullptr;
        while (cell != nullptr && cell->objectName() != QStringLiteral("sendOptionCell"))
        {
            cell = cell->parentWidget();
        }
        return cell;
    }

    QWidget *parameterCellForEditor(QWidget *editor)
    {
        QWidget *cell = editor != nullptr ? editor->parentWidget() : nullptr;
        while (cell != nullptr && cell->objectName() != QStringLiteral("sendParameterCell"))
        {
            cell = cell->parentWidget();
        }
        return cell;
    }

    bool isAncestorOf(const QWidget *ancestor, const QWidget *child)
    {
        for (const QWidget *current = child; current != nullptr; current = current->parentWidget())
        {
            if (current == ancestor)
            {
                return true;
            }
        }
        return false;
    }

    DataPacket receivePacket(const QByteArray &payload, qint64 timestamp = 123456)
    {
        DataPacket packet;
        packet.timestamp = timestamp;
        packet.rawPayload = payload;
        packet.metadata.insert(QStringLiteral("direction"), QStringLiteral("rx"));
        return packet;
    }
}

class AppWidgetTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("est-widget-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("quick-command-panel-tests"));
        const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir(appDataPath).removeRecursively();
    }

    void quickCommandPanelShowsOnlyListAndEditActions()
    {
        QuickCommandPanel panel;

        QCOMPARE(panel.findChildren<QLineEdit *>().size(), 0);
        QVERIFY(panel.findChild<QListWidget *>() != nullptr);

        QStringList buttonLabels;
        const QList<QPushButton *> buttons = panel.findChildren<QPushButton *>();
        for (const QPushButton *button : buttons)
        {
            buttonLabels.append(button->text());
        }
        buttonLabels.sort();

        QCOMPARE(buttonLabels, QStringList({QStringLiteral("删除"),
                                            QStringLiteral("新增"),
                                            QStringLiteral("编辑")}));
    }

    void quickCommandPanelUsesTallerCommandList()
    {
        QuickCommandPanel panel;
        auto *listWidget = panel.findChild<QListWidget *>();

        QVERIFY(listWidget != nullptr);
        QVERIFY(listWidget->maximumHeight() >= 620);
    }

    void quickCommandPanelDisplaysCommandsAsNamePipeContent()
    {
        const QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(appDataPath);
        QFile file(QDir(appDataPath).filePath(QStringLiteral("quick_commands.json")));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(R"({"commands":[{"name":"JCID_XBOX_TEST","content":"JC_xbox_test","mode":"text","appendNewLine":true,"lineEnding":"crlf","remark":"ignored"}]})");
        file.close();

        QuickCommandPanel panel;
        auto *listWidget = panel.findChild<QListWidget *>();

        QVERIFY(listWidget != nullptr);
        QCOMPARE(listWidget->count(), 1);
        QCOMPARE(listWidget->item(0)->text(), QStringLiteral("JCID_XBOX_TEST|JC_xbox_test"));
    }

    void quickCommandAddDialogOnlyAsksForNameAndContent()
    {
        QuickCommandPanel panel;
        QPushButton *addButton = nullptr;
        const QList<QPushButton *> buttons = panel.findChildren<QPushButton *>();
        for (QPushButton *button : buttons)
        {
            if (button->text() == QStringLiteral("新增"))
            {
                addButton = button;
                break;
            }
        }
        QVERIFY(addButton != nullptr);

        bool dialogSeen = false;
        int lineEditCount = -1;
        int textEditCount = -1;
        int checkBoxCount = -1;

        QTimer::singleShot(0, [&]() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (dialog != nullptr)
            {
                dialogSeen = true;
                lineEditCount = dialog->findChildren<QLineEdit*>().size();
                textEditCount = dialog->findChildren<QPlainTextEdit*>().size();
                checkBoxCount = dialog->findChildren<QCheckBox*>().size();
                dialog->reject();
            }
        });

        addButton->click();

        QVERIFY(dialogSeen);
        QCOMPARE(lineEditCount, 1);
        QCOMPARE(textEditCount, 1);
        QCOMPARE(checkBoxCount, 0);
    }

    void serialPortComboUsesBelowPopupPlacement()
    {
        SerialConfigBar bar;

        auto *portCombo = bar.findChild<QComboBox *>(QStringLiteral("serialPortCombo"));

        QVERIFY(portCombo != nullptr);
        QCOMPARE(portCombo->property("popupPlacement").toString(), QStringLiteral("below"));
    }

    void serialReceiveToolbarMovesSettingsDownAndRemovesTimestampToggle()
    {
        SerialReceiveView view;

        auto *titleLabel = view.findChild<QLabel *>(QStringLiteral("receiveSectionTitle"));
        QVERIFY(titleLabel != nullptr);
        QCOMPARE(titleLabel->text(), QStringLiteral("数据区"));

        QStringList checkBoxLabels;
        for (const QCheckBox *checkBox : view.findChildren<QCheckBox *>())
        {
            checkBoxLabels.append(checkBox->text());
        }
        QVERIFY(!checkBoxLabels.contains(QStringLiteral("时间戳")));

        QPushButton *settingsButton = nullptr;
        for (QPushButton *button : view.findChildren<QPushButton *>())
        {
            if (button->text() == QStringLiteral("设置"))
            {
                settingsButton = button;
                break;
            }
        }
        QVERIFY(settingsButton != nullptr);

        QTimer::singleShot(0, [&]() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog != nullptr);
            dialog->reject();
        });
        settingsButton->click();
    }

    void serialReceiveEntriesIncludeTimestampPrefix()
    {
        SerialReceiveView view;
        const DataPacket packet = receivePacket(QByteArrayLiteral("abc"));

        view.appendPacket(packet);

        auto *textEdit = view.findChild<QPlainTextEdit *>();
        QVERIFY(textEdit != nullptr);
        QVERIFY(textEdit->toPlainText().startsWith(QStringLiteral("[")));
        QVERIFY(!textEdit->toPlainText().startsWith(QStringLiteral("[1970-")));
        QVERIFY(textEdit->toPlainText().contains(QStringLiteral("] RX: abc")));
    }

    void serialReceiveAutoScrollCheckboxControlsScrollPosition()
    {
        SerialReceiveView view;
        view.resize(420, 160);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));

        auto *textEdit = view.findChild<QPlainTextEdit *>(QStringLiteral("serialReceiveTextEdit"));
        QVERIFY(textEdit != nullptr);
        auto *autoScrollCheckBox = view.findChild<QCheckBox *>();
        QVERIFY(autoScrollCheckBox != nullptr);

        for (int i = 0; i < 80; ++i)
        {
            view.appendPacket(receivePacket(QStringLiteral("line %1").arg(i).toUtf8()));
        }
        QApplication::processEvents();

        auto *scrollBar = textEdit->verticalScrollBar();
        scrollBar->setValue(0);
        const int topPosition = scrollBar->value();
        autoScrollCheckBox->setChecked(false);
        view.appendPacket(receivePacket(QByteArrayLiteral("keep position")));
        QApplication::processEvents();

        QCOMPARE(scrollBar->value(), topPosition);

        autoScrollCheckBox->setChecked(true);
        view.appendPacket(receivePacket(QByteArrayLiteral("scroll bottom")));
        QApplication::processEvents();

        QCOMPARE(scrollBar->value(), scrollBar->maximum());
    }

    void serialReceiveFixedFieldsUseCompactPaintedBadges()
    {
        SerialReceiveView view;
        DataPacket rxPacket;
        rxPacket.timestamp = 123456;
        rxPacket.rawPayload = QByteArrayLiteral("abc");
        rxPacket.metadata.insert(QStringLiteral("direction"), QStringLiteral("rx"));
        rxPacket.metadata.insert(QStringLiteral("portName"), QStringLiteral("COM_RX"));
        DataPacket txPacket = rxPacket;
        txPacket.metadata.insert(QStringLiteral("direction"), QStringLiteral("tx"));
        txPacket.metadata.insert(QStringLiteral("portName"), QStringLiteral("COM_TX"));

        view.appendPacket(rxPacket);
        view.appendPacket(txPacket);

        auto *textEdit = view.findChild<QPlainTextEdit *>(QStringLiteral("serialReceiveTextEdit"));
        QVERIFY(textEdit != nullptr);

        const QColor timestampColor = foregroundColorForToken(textEdit, QStringLiteral("["));
        const QColor rxColor = foregroundColorForToken(textEdit, QStringLiteral("RX"));
        const QColor txColor = foregroundColorForToken(textEdit, QStringLiteral("TX"));
        const QColor comColor = foregroundColorForToken(textEdit, QStringLiteral("COM_RX"));

        QVERIFY(timestampColor.isValid());
        QVERIFY(rxColor.isValid());
        QVERIFY(txColor.isValid());
        QVERIFY(comColor.isValid());
        QCOMPARE(rxColor, QColor(QStringLiteral("#006B3F")));
        QCOMPARE(txColor, QColor(QStringLiteral("#B91C1C")));
        QCOMPARE(comColor, QColor(QStringLiteral("#5B21B6")));
        QVERIFY(!backgroundColorForToken(textEdit, QStringLiteral("RX")).isValid());
        QVERIFY(!backgroundColorForToken(textEdit, QStringLiteral("TX")).isValid());
        QVERIFY(!backgroundColorForToken(textEdit, QStringLiteral("COM_RX")).isValid());
    }

    void serialReceiveSettingsCanDisableFixedFieldHighlight()
    {
        SerialReceiveView view;

        QTimer::singleShot(0, [&]() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog != nullptr);
            auto *highlightCheckBox = dialog->findChild<QCheckBox *>(QStringLiteral("receiveHighlightCheckBox"));
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            QVERIFY(highlightCheckBox != nullptr);
            QVERIFY(buttons != nullptr);
            highlightCheckBox->setChecked(false);
            buttons->button(QDialogButtonBox::Ok)->click();
        });

        QVERIFY(view.showDisplaySettingsDialog());

        DataPacket packet;
        packet.timestamp = 123456;
        packet.rawPayload = QByteArrayLiteral("abc");
        packet.metadata.insert(QStringLiteral("direction"), QStringLiteral("rx"));
        view.appendPacket(packet);

        auto *textEdit = view.findChild<QPlainTextEdit *>(QStringLiteral("serialReceiveTextEdit"));
        QVERIFY(textEdit != nullptr);
        QVERIFY(!foregroundColorForToken(textEdit, QStringLiteral("RX")).isValid());
    }

    void serialReceiveTextZoomsWithControlWheel()
    {
        SerialReceiveView view;
        auto *textEdit = view.findChild<QPlainTextEdit *>(QStringLiteral("serialReceiveTextEdit"));

        QVERIFY(textEdit != nullptr);
        const qreal originalPointSize = textEdit->font().pointSizeF();

        QWheelEvent zoomInEvent(QPointF(8, 8),
                                QPointF(8, 8),
                                QPoint(),
                                QPoint(0, 120),
                                Qt::NoButton,
                                Qt::ControlModifier,
                                Qt::NoScrollPhase,
                                false);
        QApplication::sendEvent(textEdit->viewport(), &zoomInEvent);

        QVERIFY(textEdit->font().pointSizeF() > originalPointSize);

        QWheelEvent zoomOutEvent(QPointF(8, 8),
                                 QPointF(8, 8),
                                 QPoint(),
                                 QPoint(0, -120),
                                 Qt::NoButton,
                                 Qt::ControlModifier,
                                 Qt::NoScrollPhase,
                                 false);
        QApplication::sendEvent(textEdit->viewport(), &zoomOutEvent);

        QCOMPARE(textEdit->font().pointSizeF(), originalPointSize);
    }

    void serialAdvancedSettingsDialogUpdatesConfig()
    {
        SerialConfigBar bar;

        QTimer::singleShot(0, [&]() {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog != nullptr);

            auto *dataBitsCombo = dialog->findChild<QComboBox *>(QStringLiteral("advancedDataBitsCombo"));
            auto *stopBitsCombo = dialog->findChild<QComboBox *>(QStringLiteral("advancedStopBitsCombo"));
            auto *parityCombo = dialog->findChild<QComboBox *>(QStringLiteral("advancedParityCombo"));
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            QVERIFY(dataBitsCombo != nullptr);
            QVERIFY(stopBitsCombo != nullptr);
            QVERIFY(parityCombo != nullptr);
            QVERIFY(buttons != nullptr);

            dataBitsCombo->setCurrentText(QStringLiteral("7"));
            stopBitsCombo->setCurrentIndex(stopBitsCombo->findData(QStringLiteral("2")));
            parityCombo->setCurrentIndex(parityCombo->findData(QStringLiteral("even")));
            buttons->button(QDialogButtonBox::Ok)->click();
        });

        QVERIFY(bar.showAdvancedSettingsDialog());

        const QVariantMap config = bar.currentConfig();
        QCOMPARE(config.value(QStringLiteral("dataBits")).toInt(), 7);
        QCOMPARE(config.value(QStringLiteral("stopBits")).toString(), QStringLiteral("2"));
        QCOMPARE(config.value(QStringLiteral("parity")).toString(), QStringLiteral("even"));
    }

    void serialAssistantShowsSentPayloadInDataArea()
    {
        FakeCore core;
        SerialAssistantPage page(&core);

        auto *configBar = page.findChild<SerialConfigBar *>(QStringLiteral("serialConfigBar"));
        auto *sendInput = page.findChild<QPlainTextEdit *>(QStringLiteral("sendInputEdit"));
        auto *receiveText = page.findChild<QPlainTextEdit *>(QStringLiteral("serialReceiveTextEdit"));
        QVERIFY(configBar != nullptr);
        QVERIFY(sendInput != nullptr);
        QVERIFY(receiveText != nullptr);

        configBar->setPortItems({{QStringLiteral("COM_TEST - Fake"), QStringLiteral("COM_TEST")}},
                                QStringLiteral("COM_TEST"));

        QPushButton *openButton = nullptr;
        QPushButton *sendButton = nullptr;
        QPushButton *sendClearButton = nullptr;
        for (QPushButton *button : page.findChildren<QPushButton *>())
        {
            if (button->objectName() == QStringLiteral("openSerialButton"))
            {
                openButton = button;
            }
            if (button->property("role").toString() == QStringLiteral("sendPayload"))
            {
                sendButton = button;
            }
            if (button->objectName() == QStringLiteral("clearSendInputButton"))
            {
                sendClearButton = button;
            }
        }
        QVERIFY(openButton != nullptr);
        QVERIFY(sendButton != nullptr);
        QVERIFY(sendClearButton != nullptr);

        openButton->click();
        sendInput->setPlainText(QStringLiteral("ping"));
        sendButton->click();

        QVERIFY(receiveText->toPlainText().contains(QStringLiteral("TX COM_TEST: ping")));

        sendClearButton->click();

        QCOMPARE(receiveText->toPlainText(), QString());
        QCOMPARE(sendInput->toPlainText(), QStringLiteral("ping"));
    }

    void serialAssistantTimestampToggleControlsDataAreaTimestamp()
    {
        FakeCore core;
        SerialAssistantPage page(&core);

        auto *configBar = page.findChild<SerialConfigBar *>(QStringLiteral("serialConfigBar"));
        auto *sendInput = page.findChild<QPlainTextEdit *>(QStringLiteral("sendInputEdit"));
        auto *receiveText = page.findChild<QPlainTextEdit *>(QStringLiteral("serialReceiveTextEdit"));
        auto *clearReceiveButton = page.findChild<QPushButton *>(QStringLiteral("receiveClearDataButton"));
        auto *timestampToggle = findCheckBoxByText(&page, QStringLiteral("时间戳"));

        QVERIFY(configBar != nullptr);
        QVERIFY(sendInput != nullptr);
        QVERIFY(receiveText != nullptr);
        QVERIFY(clearReceiveButton != nullptr);
        QVERIFY(timestampToggle != nullptr);

        configBar->setPortItems({{QStringLiteral("COM_TEST - Fake"), QStringLiteral("COM_TEST")}},
                                QStringLiteral("COM_TEST"));

        QPushButton *openButton = nullptr;
        QPushButton *sendButton = nullptr;
        for (QPushButton *button : page.findChildren<QPushButton *>())
        {
            if (button->objectName() == QStringLiteral("openSerialButton"))
            {
                openButton = button;
            }
            if (button->property("role").toString() == QStringLiteral("sendPayload"))
            {
                sendButton = button;
            }
        }
        QVERIFY(openButton != nullptr);
        QVERIFY(sendButton != nullptr);

        openButton->click();
        if (timestampToggle->isChecked())
        {
            timestampToggle->click();
        }
        QVERIFY(!timestampToggle->isChecked());

        sendInput->setPlainText(QStringLiteral("ping"));
        sendButton->click();
        QVERIFY(receiveText->toPlainText().startsWith(QStringLiteral("TX COM_TEST: ping")));

        clearReceiveButton->click();
        timestampToggle->click();
        sendInput->setPlainText(QStringLiteral("pong"));
        sendButton->click();
        QVERIFY(receiveText->toPlainText().startsWith(QStringLiteral("[")));
        QVERIFY(receiveText->toPlainText().contains(QStringLiteral("] TX COM_TEST: pong")));
    }

    void serialReceiveClearResetsTransferStats()
    {
        FakeCore core;
        SerialAssistantPage page(&core);

        auto *configBar = page.findChild<SerialConfigBar *>(QStringLiteral("serialConfigBar"));
        auto *sendInput = page.findChild<QPlainTextEdit *>(QStringLiteral("sendInputEdit"));
        auto *receiveText = page.findChild<QPlainTextEdit *>(QStringLiteral("serialReceiveTextEdit"));
        auto *clearReceiveButton = page.findChild<QPushButton *>(QStringLiteral("receiveClearDataButton"));

        QVERIFY(configBar != nullptr);
        QVERIFY(sendInput != nullptr);
        QVERIFY(receiveText != nullptr);
        QVERIFY(clearReceiveButton != nullptr);

        configBar->setPortItems({{QStringLiteral("COM_TEST - Fake"), QStringLiteral("COM_TEST")}},
                                QStringLiteral("COM_TEST"));

        QPushButton *openButton = nullptr;
        QPushButton *sendButton = nullptr;
        for (QPushButton *button : page.findChildren<QPushButton *>())
        {
            if (button->objectName() == QStringLiteral("openSerialButton"))
            {
                openButton = button;
            }
            if (button->property("role").toString() == QStringLiteral("sendPayload"))
            {
                sendButton = button;
            }
        }
        QVERIFY(openButton != nullptr);
        QVERIFY(sendButton != nullptr);

        QSignalSpy statsSpy(&page, &SerialAssistantPage::transferStatsChanged);

        openButton->click();
        sendInput->setPlainText(QStringLiteral("ping"));
        sendButton->click();

        QVERIFY(receiveText->toPlainText().contains(QStringLiteral("TX COM_TEST: ping")));
        QVERIFY(!statsSpy.isEmpty());
        const QList<QVariant> sentStats = statsSpy.takeLast();
        QVERIFY(sentStats.at(0).toLongLong() > 0);

        clearReceiveButton->click();

        QCOMPARE(receiveText->toPlainText(), QString());
        QVERIFY(!statsSpy.isEmpty());
        const QList<QVariant> clearStats = statsSpy.takeLast();
        QCOMPARE(clearStats.at(0).toLongLong(), 0);
        QCOMPARE(clearStats.at(1).toLongLong(), 0);
    }

    void serialReceiveAndSendAreasUseSameWidth()
    {
        FakeCore core;
        SerialAssistantPage page(&core);
        page.resize(1000, 760);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QApplication::processEvents();

        auto *receiveView = page.findChild<SerialReceiveView *>(QStringLiteral("serialReceiveView"));
        auto *sendPanel = page.findChild<SerialSendPanel *>(QStringLiteral("serialSendPanel"));
        auto *quickCommandPanel = page.findChild<QuickCommandPanel *>(QStringLiteral("quickCommandPanel"));
        auto *rightSettingsPanel = page.findChild<QWidget *>(QStringLiteral("serialRightSettingsPanel"));
        auto *rightOptionPanel = page.findChild<QWidget *>(QStringLiteral("serialRightOptionPanel"));
        auto *sendControlPanel = page.findChild<QWidget *>(QStringLiteral("sendControlPanel"));
        auto *sendOptionArea = page.findChild<QWidget *>(QStringLiteral("sendOptionArea"));

        QVERIFY(receiveView != nullptr);
        QVERIFY(sendPanel != nullptr);
        QVERIFY(quickCommandPanel != nullptr);
        QVERIFY(rightSettingsPanel != nullptr);
        QVERIFY(rightOptionPanel != nullptr);
        QVERIFY(sendControlPanel != nullptr);
        QVERIFY(sendOptionArea != nullptr);
        const QRect receiveRect(receiveView->mapTo(&page, QPoint(0, 0)), receiveView->size());
        const QRect sendRect(sendPanel->mapTo(&page, QPoint(0, 0)), sendPanel->size());
        const QRect quickCommandRect(quickCommandPanel->mapTo(&page, QPoint(0, 0)), quickCommandPanel->size());
        const QRect settingsRect(rightSettingsPanel->mapTo(&page, QPoint(0, 0)), rightSettingsPanel->size());
        const QRect optionPanelRect(rightOptionPanel->mapTo(&page, QPoint(0, 0)), rightOptionPanel->size());
        const QRect sendControlsRect(sendControlPanel->mapTo(&page, QPoint(0, 0)), sendControlPanel->size());
        const QRect sendOptionsRect(sendOptionArea->mapTo(&page, QPoint(0, 0)), sendOptionArea->size());

        QCOMPARE(sendRect.x(), receiveRect.x());
        QCOMPARE(sendPanel->geometry().width(), receiveView->geometry().width());
        QVERIFY(sendRect.right() < quickCommandRect.left());
        QCOMPARE(settingsRect.x(), quickCommandRect.x());
        QCOMPARE(settingsRect.width(), quickCommandRect.width());
        QCOMPARE(optionPanelRect.x(), quickCommandRect.x());
        QCOMPARE(optionPanelRect.width(), quickCommandRect.width());
        QCOMPARE(settingsRect.y(), sendRect.y());
        QCOMPARE(settingsRect.height(), sendRect.height());
        QVERIFY(settingsRect.contains(sendControlsRect));
        QVERIFY(optionPanelRect.contains(sendOptionsRect));
        QVERIFY(optionPanelRect.top() > quickCommandRect.bottom());
        QVERIFY(optionPanelRect.top() <= quickCommandRect.bottom() + 9);
        QVERIFY(optionPanelRect.bottom() < settingsRect.top());
        QVERIFY(optionPanelRect.height() >= 244);
        QVERIFY(sendOptionsRect.width() >= optionPanelRect.width() - 12);
        QVERIFY(sendControlsRect.left() >= settingsRect.left());
        QVERIFY(sendControlsRect.right() <= settingsRect.right());
        QVERIFY(sendControlsRect.top() <= settingsRect.top() + 4);
        QVERIFY(sendControlsRect.bottom() >= settingsRect.bottom() - 4);
        QVERIFY(!sendControlsRect.contains(sendOptionsRect));
    }

    void serialSendControlsUseRoomierLayout()
    {
        SerialSendPanel panel;

        auto *controlPanel = panel.findChild<QWidget *>(QStringLiteral("sendControlPanel"));
        auto *timeoutSpin = panel.findChild<QSpinBox *>(QStringLiteral("sendTimeoutSpinBox"));
        auto *autoSendSpin = panel.findChild<QSpinBox *>(QStringLiteral("autoSendIntervalSpinBox"));
        auto *inputEdit = panel.findChild<QPlainTextEdit *>(QStringLiteral("sendInputEdit"));
        QLabel *timeoutUnitLabel = nullptr;
        QLabel *autoSendUnitLabel = nullptr;
        for (QLabel *label : panel.findChildren<QLabel *>())
        {
            if (label->text() == QStringLiteral("ms超时"))
            {
                timeoutUnitLabel = label;
            }
            if (label->text() == QStringLiteral("ms/次"))
            {
                autoSendUnitLabel = label;
            }
        }
        QPushButton *sendButton = nullptr;
        for (QPushButton *button : panel.findChildren<QPushButton *>())
        {
            if (button->property("role").toString() == QStringLiteral("sendPayload"))
            {
                sendButton = button;
                break;
            }
        }
        auto *clearButton = panel.findChild<QPushButton *>(QStringLiteral("clearSendInputButton"));

        QVERIFY(controlPanel != nullptr);
        QVERIFY(inputEdit != nullptr);
        QVERIFY(timeoutSpin != nullptr);
        QVERIFY(autoSendSpin != nullptr);
        QVERIFY(timeoutUnitLabel != nullptr);
        QVERIFY(autoSendUnitLabel != nullptr);
        QVERIFY(sendButton != nullptr);
        QVERIFY(clearButton != nullptr);
        QVERIFY(controlPanel->minimumWidth() >= 360);
        QVERIFY(controlPanel->minimumHeight() >= 156);
        QVERIFY(timeoutSpin->minimumWidth() >= 86);
        QVERIFY(autoSendSpin->minimumWidth() >= 86);
        QVERIFY(timeoutSpin->minimumHeight() >= 30);
        QVERIFY(autoSendSpin->minimumHeight() >= 30);
        QVERIFY(sendButton->minimumWidth() >= 320);
        QVERIFY(clearButton->minimumWidth() >= 320);
        QVERIFY(sendButton->minimumHeight() >= 64);
        QVERIFY(clearButton->minimumHeight() >= 64);
        QCOMPARE(sendButton->sizePolicy().verticalPolicy(), QSizePolicy::Expanding);
        QCOMPARE(clearButton->sizePolicy().verticalPolicy(), QSizePolicy::Expanding);

        panel.resize(760, 156);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QApplication::processEvents();

        const QRect inputRect(inputEdit->mapTo(&panel, QPoint(0, 0)), inputEdit->size());
        const QRect sendButtonRect(sendButton->mapTo(&panel, QPoint(0, 0)), sendButton->size());
        const QRect clearButtonRect(clearButton->mapTo(&panel, QPoint(0, 0)), clearButton->size());

        QCOMPARE(sendButtonRect.top(), inputRect.top());
        QCOMPARE(clearButtonRect.bottom(), inputRect.bottom());
        QCOMPARE(sendButtonRect.bottom() + 1, clearButtonRect.top());
        QVERIFY(qAbs(sendButtonRect.height() - clearButtonRect.height()) <= 1);
    }

    void serialSendOptionsUseButtonLikeToggleArea()
    {
        SerialSendPanel panel;
        panel.resize(500, 180);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QApplication::processEvents();

        const QList<QCheckBox *> optionButtons = panel.findChildren<QCheckBox *>(
            QStringLiteral("sendOptionToggle"));

        QCOMPARE(optionButtons.size(), 6);
        for (QCheckBox *optionButton : optionButtons)
        {
            QCOMPARE(optionButton->property("sendOption").toBool(), true);
            QVERIFY(optionButton->minimumWidth() >= 112);
            QVERIFY(optionButton->minimumHeight() >= 38);
        }

        auto *timestampToggle = findCheckBoxByText(&panel, QStringLiteral("时间戳"));
        auto *loopSendToggle = findCheckBoxByText(&panel, QStringLiteral("循环发送"));
        auto *hexDisplayToggle = findCheckBoxByText(&panel, QStringLiteral("HEX显示"));
        auto *hexSendToggle = findCheckBoxByText(&panel, QStringLiteral("HEX发送"));
        auto *enterSendToggle = findCheckBoxByText(&panel, QStringLiteral("回车发送"));
        auto *appendNewLineToggle = findCheckBoxByText(&panel, QStringLiteral("追加新行"));
        auto *timeoutSpin = panel.findChild<QSpinBox *>(QStringLiteral("sendTimeoutSpinBox"));
        auto *autoSendSpin = panel.findChild<QSpinBox *>(QStringLiteral("autoSendIntervalSpinBox"));
        QLabel *timeoutUnitLabel = nullptr;
        QLabel *autoSendUnitLabel = nullptr;
        for (QLabel *label : panel.findChildren<QLabel *>())
        {
            if (label->text() == QStringLiteral("ms超时"))
            {
                timeoutUnitLabel = label;
            }
            if (label->text() == QStringLiteral("ms/次"))
            {
                autoSendUnitLabel = label;
            }
        }

        QVERIFY(timestampToggle != nullptr);
        QCOMPARE(timestampToggle->isChecked(), true);
        QVERIFY(loopSendToggle != nullptr);
        QVERIFY(hexDisplayToggle != nullptr);
        QVERIFY(hexSendToggle != nullptr);
        QVERIFY(enterSendToggle != nullptr);
        QVERIFY(appendNewLineToggle != nullptr);
        QVERIFY(timeoutSpin != nullptr);
        QVERIFY(autoSendSpin != nullptr);
        QVERIFY(timeoutUnitLabel != nullptr);
        QVERIFY(autoSendUnitLabel != nullptr);
        QCOMPARE(hexDisplayToggle->geometry().width(), hexSendToggle->geometry().width());
        QCOMPARE(enterSendToggle->geometry().width(), appendNewLineToggle->geometry().width());
        QCOMPARE(timestampToggle->geometry().width(), loopSendToggle->geometry().width());
        auto *timestampCell = optionCellForToggle(timestampToggle);
        auto *loopSendCell = optionCellForToggle(loopSendToggle);
        auto *timeoutCell = parameterCellForEditor(timeoutSpin);
        auto *autoSendIntervalCell = parameterCellForEditor(autoSendSpin);
        auto *hexDisplayCell = optionCellForToggle(hexDisplayToggle);
        auto *hexSendCell = optionCellForToggle(hexSendToggle);
        auto *enterSendCell = optionCellForToggle(enterSendToggle);
        auto *appendNewLineCell = optionCellForToggle(appendNewLineToggle);
        QVERIFY(timestampCell != nullptr);
        QVERIFY(loopSendCell != nullptr);
        QVERIFY(timeoutCell != nullptr);
        QVERIFY(autoSendIntervalCell != nullptr);
        QCOMPARE(timeoutCell->property("sendParameter").toBool(), true);
        QCOMPARE(autoSendIntervalCell->property("sendParameter").toBool(), true);
        QVERIFY(isAncestorOf(timeoutCell, timeoutUnitLabel));
        QVERIFY(isAncestorOf(autoSendIntervalCell, autoSendUnitLabel));
        QVERIFY(hexDisplayCell != nullptr);
        QVERIFY(hexSendCell != nullptr);
        QVERIFY(enterSendCell != nullptr);
        QVERIFY(appendNewLineCell != nullptr);
        QCOMPARE(timestampCell->geometry().width(), timeoutCell->geometry().width());
        QCOMPARE(loopSendCell->geometry().width(), autoSendIntervalCell->geometry().width());
        QCOMPARE(hexDisplayCell->geometry().width(), hexSendCell->geometry().width());
        QCOMPARE(enterSendCell->geometry().width(), appendNewLineCell->geometry().width());
        QCOMPARE(timestampCell->geometry().y(), timeoutCell->geometry().y());
        QCOMPARE(loopSendCell->geometry().y(), autoSendIntervalCell->geometry().y());
        QVERIFY(loopSendCell->geometry().y() > timestampCell->geometry().y());
        QVERIFY(hexDisplayCell->geometry().y() > loopSendCell->geometry().y());
        QCOMPARE(panel.autoSendEnabled(), false);

        loopSendToggle->click();

        QCOMPARE(loopSendToggle->isChecked(), true);
        QCOMPARE(panel.autoSendEnabled(), true);

        const QPoint cellBlankPoint(loopSendCell->width() - 8, loopSendCell->height() / 2);
        QTest::mouseClick(loopSendCell, Qt::LeftButton, Qt::NoModifier, cellBlankPoint);
        QCOMPARE(loopSendToggle->isChecked(), false);
        QCOMPARE(panel.autoSendEnabled(), false);
    }

    void serialSendOptionButtonsPerformTheirFunctions()
    {
        SerialSendPanel panel;

        auto *inputEdit = panel.findChild<QPlainTextEdit *>(QStringLiteral("sendInputEdit"));
        auto *timestampToggle = findCheckBoxByText(&panel, QStringLiteral("时间戳"));
        auto *hexDisplayToggle = findCheckBoxByText(&panel, QStringLiteral("HEX显示"));
        auto *hexSendToggle = findCheckBoxByText(&panel, QStringLiteral("HEX发送"));
        auto *enterSendToggle = findCheckBoxByText(&panel, QStringLiteral("回车发送"));
        auto *appendNewLineToggle = findCheckBoxByText(&panel, QStringLiteral("追加新行"));
        auto *clearButton = panel.findChild<QPushButton *>(QStringLiteral("clearSendInputButton"));

        QVERIFY(inputEdit != nullptr);
        QVERIFY(timestampToggle != nullptr);
        QVERIFY(hexDisplayToggle != nullptr);
        QVERIFY(hexSendToggle != nullptr);
        QVERIFY(enterSendToggle != nullptr);
        QVERIFY(appendNewLineToggle != nullptr);
        QVERIFY(clearButton != nullptr);

        QString errorMessage;
        inputEdit->setPlainText(QStringLiteral("abc"));
        QCOMPARE(panel.buildPayload(&errorMessage), QByteArrayLiteral("abc\r\n"));
        appendNewLineToggle->click();
        QCOMPARE(panel.buildPayload(&errorMessage), QByteArrayLiteral("abc"));

        hexSendToggle->click();
        inputEdit->setPlainText(QStringLiteral("41 42"));
        QCOMPARE(panel.buildPayload(&errorMessage), QByteArrayLiteral("AB"));

        hexSendToggle->click();
        inputEdit->setPlainText(QStringLiteral("AB"));
        hexDisplayToggle->click();
        QCOMPARE(inputEdit->toPlainText(), QStringLiteral("41 42"));
        hexDisplayToggle->click();
        QCOMPARE(inputEdit->toPlainText(), QStringLiteral("AB"));

        inputEdit->setPlainText(QStringLiteral("abc"));
        QVERIFY(timestampToggle->isChecked());
        const QByteArray timestampPayload = panel.buildPayload(&errorMessage);
        QCOMPARE(timestampPayload, QByteArrayLiteral("abc"));
        QCOMPARE(panel.timestampEnabled(), true);

        QSignalSpy sendSpy(&panel, &SerialSendPanel::sendRequested);
        enterSendToggle->click();
        inputEdit->setFocus();
        QTest::keyClick(inputEdit, Qt::Key_Return);
        QCOMPARE(sendSpy.count(), 1);

        QSignalSpy clearSpy(&panel, &SerialSendPanel::clearDataRequested);
        clearButton->click();
        QCOMPARE(inputEdit->toPlainText(), QStringLiteral("abc"));
        QCOMPARE(clearSpy.count(), 1);
    }

    void binDataInspectorParsesLittleAndBigEndianValues()
    {
        const QByteArray bytes = QByteArray::fromHex("010203040000803f000000000000f03f");

        const auto littleRows = ByteDataInspectorService::inspect(bytes, 0, ByteDataInspectorService::Endian::Little);
        const auto bigRows = ByteDataInspectorService::inspect(bytes, 0, ByteDataInspectorService::Endian::Big);
        const auto floatRows = ByteDataInspectorService::inspect(bytes, 4, ByteDataInspectorService::Endian::Little);
        const auto doubleRows = ByteDataInspectorService::inspect(bytes, 8, ByteDataInspectorService::Endian::Little);

        QCOMPARE(littleRows.value(QStringLiteral("uint16")), QStringLiteral("513 (0x0201)"));
        QCOMPARE(bigRows.value(QStringLiteral("uint16")), QStringLiteral("258 (0x0102)"));
        QCOMPARE(littleRows.value(QStringLiteral("uint32")), QStringLiteral("67305985 (0x04030201)"));
        QCOMPARE(bigRows.value(QStringLiteral("uint32")), QStringLiteral("16909060 (0x01020304)"));
        QCOMPARE(floatRows.value(QStringLiteral("float")), QStringLiteral("1"));
        QCOMPARE(doubleRows.value(QStringLiteral("double")), QStringLiteral("1"));
    }

    void binDataInspectorMarksUnavailableValuesAtEnd()
    {
        const QByteArray bytes = QByteArray::fromHex("AA BB");

        const auto rows = ByteDataInspectorService::inspect(bytes, 1, ByteDataInspectorService::Endian::Little);

        QCOMPARE(rows.value(QStringLiteral("uint8")), QStringLiteral("187 (0xBB)"));
        QCOMPARE(rows.value(QStringLiteral("uint16")), QStringLiteral("-"));
        QCOMPARE(rows.value(QStringLiteral("double")), QStringLiteral("-"));
    }

    void binChecksumServiceCalculatesKnownVectors()
    {
        const QByteArray digits("123456789");
        const auto checks = ByteChecksumService::calculate(digits);

        QCOMPARE(checks.value(QStringLiteral("CheckSum-8")), QStringLiteral("0xDD"));
        QCOMPARE(checks.value(QStringLiteral("CheckSum-16")), QStringLiteral("0x01DD"));
        QCOMPARE(checks.value(QStringLiteral("CRC-8")), QStringLiteral("0xF4"));
        QCOMPARE(checks.value(QStringLiteral("CRC-16/CCITT-FALSE")), QStringLiteral("0x29B1"));
        QCOMPARE(checks.value(QStringLiteral("CRC-32/IEEE")), QStringLiteral("0xCBF43926"));

        const auto hashes = ByteChecksumService::calculate(QByteArrayLiteral("abc"));
        QCOMPARE(hashes.value(QStringLiteral("MD5")), QStringLiteral("900150983CD24FB0D6963F7D28E17F72"));
        QCOMPARE(hashes.value(QStringLiteral("SHA-1")), QStringLiteral("A9993E364706816ABA3E25717850C26C9CD0D89D"));
    }

    void dataConvertServiceFormatsCArray()
    {
        const QString text = ByteFormatService::formatCArray(QByteArray::fromHex("0102ff"), QStringLiteral("firmware"), 2);

        QCOMPARE(text, QStringLiteral("const uint8_t firmware[] = {\n    0x01, 0x02,\n    0xFF\n};"));
    }

    void dataConvertServiceSupportsBase64AndDecimalByteLists()
    {
        DataConvertService::ConversionOptions options;
        QString output;
        QString errorMessage;

        options.inputFormat = DataConvertService::DataFormat::Hex;
        options.outputFormat = DataConvertService::DataFormat::Base64;
        QVERIFY(DataConvertService::convert(QStringLiteral("4A 43 58 58"), options, &output, &errorMessage));
        QCOMPARE(output, QStringLiteral("SkNYWA=="));

        options.inputFormat = DataConvertService::DataFormat::Base64;
        options.outputFormat = DataConvertService::DataFormat::DecimalBytes;
        QVERIFY(DataConvertService::convert(QStringLiteral("SkNYWA=="), options, &output, &errorMessage));
        QCOMPARE(output, QStringLiteral("74 67 88 88"));

        options.inputFormat = DataConvertService::DataFormat::DecimalBytes;
        options.outputFormat = DataConvertService::DataFormat::String;
        QVERIFY(DataConvertService::convert(QStringLiteral("74, 67, 88, 88"), options, &output, &errorMessage));
        QCOMPARE(output, QStringLiteral("JCXX"));
    }

    void dataConvertWidgetFocusesOnFormatConversion()
    {
        DataConvertWidget widget;

        QStringList comboKeys;
        for (QComboBox *comboBox : widget.findChildren<QComboBox *>())
        {
            for (int index = 0; index < comboBox->count(); ++index)
            {
                comboKeys.append(comboBox->itemData(index).toString());
            }
        }

        QVERIFY(comboKeys.contains(QStringLiteral("base64")));
        QVERIFY(comboKeys.contains(QStringLiteral("decimal")));
        QVERIFY(widget.findChild<QPlainTextEdit *>(QStringLiteral("convertChecksumResultEdit")) == nullptr);
        QVERIFY(widget.findChild<QWidget *>(QStringLiteral("convertInspectorTable")) == nullptr);
    }

    void enabledThemeAndPaintedAssetsUseWhiteBluePalette()
    {
        const QStringList files = {
            QStringLiteral(EST_SOURCE_DIR "/src/resources/themes/wood_classic.qss"),
            QStringLiteral(EST_SOURCE_DIR "/src/resources/themes/industrial_dark.qss"),
            QStringLiteral(EST_SOURCE_DIR "/src/app/pages/HomePage.cpp"),
            QStringLiteral(EST_SOURCE_DIR "/src/app/widgets/HexViewerWidget.cpp"),
            QStringLiteral(EST_SOURCE_DIR "/src/app/widgets/QuickCommandPanel.cpp"),
            QStringLiteral(EST_SOURCE_DIR "/src/app/widgets/SideNavBar.cpp"),
            QStringLiteral(EST_SOURCE_DIR "/src/app/widgets/IndustrialStatusCard.cpp"),
        };

        const QStringList disallowedColors = {
            QStringLiteral("#4A3E3D"),
            QStringLiteral("#F4EFE6"),
            QStringLiteral("#E8DFD1"),
            QStringLiteral("#D7C9B6"),
            QStringLiteral("#CDB9A1"),
            QStringLiteral("#3A2A28"),
            QStringLiteral("#5A433D"),
            QStringLiteral("#7B685E"),
            QStringLiteral("#DFD5C6"),
            QStringLiteral("#C1B19D"),
            QStringLiteral("#D3C5B1"),
            QStringLiteral("#B5A28D"),
            QStringLiteral("#C5B49D"),
            QStringLiteral("#8C6A56"),
            QStringLiteral("#795A48"),
            QStringLiteral("#9C7964"),
            QStringLiteral("#D0594F"),
            QStringLiteral("#BA4E45"),
            QStringLiteral("#DE645A"),
            QStringLiteral("#7A6752"),
            QStringLiteral("#EAE2D5"),
            QStringLiteral("#8C7B71"),
            QStringLiteral("#3E2F2C"),
            QStringLiteral("#7A6458"),
            QStringLiteral("#E1D6C7"),
            QStringLiteral("#F8F5EF"),
            QStringLiteral("#F7F0E7"),
            QStringLiteral("#4B332B"),
            QStringLiteral("#CFC2B0"),
            QStringLiteral("#C8B8A4"),
            QStringLiteral("#A89885"),
            QStringLiteral("#BBAA96"),
            QStringLiteral("#9D8A74"),
            QStringLiteral("#E9DFCF"),
            QStringLiteral("#2F241A"),
            QStringLiteral("#1E1E1E"),
            QStringLiteral("#252526"),
            QStringLiteral("#333333"),
            QStringLiteral("#3C3C3C"),
            QStringLiteral("#2D2D2D"),
        };

        QStringList violations;
        for (const QString &path : files)
        {
            QFile file(path);
            QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text),
                     qPrintable(QStringLiteral("Cannot open %1").arg(path)));
            const QString content = QString::fromUtf8(file.readAll()).toUpper();
            for (const QString &color : disallowedColors)
            {
                if (content.contains(color))
                {
                    violations.append(QStringLiteral("%1 contains %2").arg(path, color));
                }
            }
        }

        QVERIFY2(violations.isEmpty(), qPrintable(violations.join(QLatin1Char('\n'))));
    }
};

QTEST_MAIN(AppWidgetTests)
#include "app_widget_tests.moc"
