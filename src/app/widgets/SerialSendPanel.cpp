#include "widgets/SerialSendPanel.h"

#include "services/DataConvertService.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>

namespace est
{
    namespace
    {
        void configureSendOptionToggle(QPushButton *toggle)
        {
            toggle->setObjectName(QStringLiteral("sendOptionToggle"));
            toggle->setProperty("sendOption", true);
            toggle->setCursor(Qt::PointingHandCursor);
            toggle->setCheckable(true);
            toggle->setMinimumWidth(112);
            toggle->setMinimumHeight(38);
            toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }

        QWidget *createOptionCell(QWidget *parent, QPushButton *toggle, QWidget *extra = nullptr, QLabel *suffix = nullptr)
        {
            auto *cell = new QWidget(parent);
            cell->setObjectName(QStringLiteral("sendOptionCell"));
            cell->setCursor(Qt::PointingHandCursor);
            cell->setMinimumHeight(46);
            cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

            auto *layout = new QHBoxLayout(cell);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(8);
            layout->addWidget(toggle, 1);
            if (extra != nullptr)
            {
                layout->addWidget(extra);
            }
            if (suffix != nullptr)
            {
                suffix->setCursor(Qt::PointingHandCursor);
                layout->addWidget(suffix);
            }
            return cell;
        }

        QWidget *createParameterCell(QWidget *parent, QWidget *editor, QLabel *suffix)
        {
            auto *cell = new QWidget(parent);
            cell->setObjectName(QStringLiteral("sendParameterCell"));
            cell->setProperty("sendParameter", true);
            cell->setMinimumHeight(46);
            cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

            auto *layout = new QHBoxLayout(cell);
            layout->setContentsMargins(10, 4, 10, 4);
            layout->setSpacing(8);
            layout->addWidget(editor, 1);
            layout->addWidget(suffix);
            return cell;
        }
    }

    SerialSendPanel::SerialSendPanel(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("serialSendPanel"));

        auto *rootLayout = new QHBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(20);

        m_inputEdit = new QPlainTextEdit(this);
        m_inputEdit->setObjectName(QStringLiteral("sendInputEdit"));
        m_inputEdit->setPlaceholderText(tr("输入待发送的数据"));
        m_inputEdit->setFixedHeight(156);
        
        QFont inputFont = m_inputEdit->font();
        // Set exact 20px height for send input area: apply via pixel size, document default and stylesheet
        inputFont.setPixelSize(20);
        m_inputEdit->setFont(inputFont);
        if (m_inputEdit->document() != nullptr) {
            m_inputEdit->document()->setDefaultFont(inputFont);
        }
        m_inputEdit->setStyleSheet(QStringLiteral("font-size:20px;"));
        
        m_inputEdit->installEventFilter(this);

        m_controlPanel = new QWidget(this);
        m_controlPanel->setObjectName(QStringLiteral("sendControlPanel"));
        m_controlPanel->setFixedWidth(360);
        m_controlPanel->setMinimumHeight(156);
        auto *rightLayout = new QVBoxLayout(m_controlPanel);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(6);

        m_optionArea = new QWidget(m_controlPanel);
        m_optionArea->setObjectName(QStringLiteral("sendOptionArea"));
        m_optionArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto *grid1 = new QGridLayout(m_optionArea);
        grid1->setContentsMargins(12, 12, 12, 12);
        grid1->setHorizontalSpacing(14);
        grid1->setVerticalSpacing(10);
        grid1->setColumnStretch(0, 1);
        grid1->setColumnStretch(1, 1);

        m_timestampCheckBox = new QPushButton(tr("时间戳"), this);
        m_timestampCheckBox->setChecked(true);
        auto *timestampSpin = new QSpinBox(this);
        timestampSpin->setObjectName(QStringLiteral("sendTimeoutSpinBox"));
        timestampSpin->setMinimumWidth(86);
        timestampSpin->setMinimumHeight(38);
        timestampSpin->setRange(10, 10000);
        timestampSpin->setValue(100);
        
        m_autoSendCheckBox = new QPushButton(tr("循环发送"), this);
        m_autoSendSpinBox = new QSpinBox(this);
        m_autoSendSpinBox->setObjectName(QStringLiteral("autoSendIntervalSpinBox"));
        m_autoSendSpinBox->setMinimumWidth(86);
        m_autoSendSpinBox->setMinimumHeight(38);
        m_autoSendSpinBox->setRange(10, 60000);
        m_autoSendSpinBox->setValue(10);
        
        m_hexDisplayCheckBox = new QPushButton(tr("HEX显示"), this);
        m_hexSendCheckBox = new QPushButton(tr("HEX发送"), this);
        
        m_enterSendCheckBox = new QPushButton(tr("回车发送"), this);
        m_appendNewLineCheckBox = new QPushButton(tr("追加新行"), this);
        m_appendNewLineCheckBox->setChecked(true);

        const QList<QPushButton *> optionToggles = {
            m_timestampCheckBox,
            m_autoSendCheckBox,
            m_hexDisplayCheckBox,
            m_hexSendCheckBox,
            m_enterSendCheckBox,
            m_appendNewLineCheckBox,
        };
        for (QPushButton *toggle : optionToggles)
        {
            configureSendOptionToggle(toggle);
        }

        auto *timestampLabel = new QLabel("ms超时", this);
        auto *autoSendLabel = new QLabel("ms/次", this);
        auto *timestampCell = createOptionCell(m_optionArea, m_timestampCheckBox);
        auto *timeoutCell = createParameterCell(m_optionArea, timestampSpin, timestampLabel);
        auto *autoSendCell = createOptionCell(m_optionArea, m_autoSendCheckBox);
        auto *autoSendIntervalCell = createParameterCell(m_optionArea, m_autoSendSpinBox, autoSendLabel);
        auto *hexDisplayCell = createOptionCell(m_optionArea, m_hexDisplayCheckBox);
        auto *hexSendCell = createOptionCell(m_optionArea, m_hexSendCheckBox);
        auto *enterSendCell = createOptionCell(m_optionArea, m_enterSendCheckBox);
        auto *appendNewLineCell = createOptionCell(m_optionArea, m_appendNewLineCheckBox);

        const QList<QPair<QWidget *, QPushButton *>> optionCells = {
            {timestampCell, m_timestampCheckBox},
            {autoSendCell, m_autoSendCheckBox},
            {hexDisplayCell, m_hexDisplayCheckBox},
            {hexSendCell, m_hexSendCheckBox},
            {enterSendCell, m_enterSendCheckBox},
            {appendNewLineCell, m_appendNewLineCheckBox},
        };
        for (const auto &optionCell : optionCells)
        {
            optionCell.first->installEventFilter(this);
            m_optionCells.insert(optionCell.first, optionCell.second);
        }

        grid1->addWidget(timestampCell, 0, 0);
        grid1->addWidget(timeoutCell, 0, 1);
        
        grid1->addWidget(autoSendCell, 1, 0);
        grid1->addWidget(autoSendIntervalCell, 1, 1);
        
        grid1->addWidget(hexDisplayCell, 2, 0);
        grid1->addWidget(hexSendCell, 2, 1);
        
        grid1->addWidget(enterSendCell, 3, 0);
        grid1->addWidget(appendNewLineCell, 3, 1);

        auto *btnLayout = new QVBoxLayout();
        btnLayout->setContentsMargins(0, 0, 0, 0);
        btnLayout->setSpacing(6);
        auto *sendButton = new QPushButton(tr("发送"), this);
        sendButton->setObjectName(QStringLiteral("primaryActionButton"));
        sendButton->setProperty("role", QStringLiteral("sendPayload"));
        sendButton->setMinimumWidth(340);
        sendButton->setMinimumHeight(68);
        sendButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        auto *clearButton = new QPushButton(tr("清空"), this);
        clearButton->setObjectName(QStringLiteral("clearSendInputButton"));
        clearButton->setMinimumWidth(340);
        clearButton->setMinimumHeight(68);
        clearButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        
        btnLayout->addWidget(sendButton, 1);
        btnLayout->addWidget(clearButton, 1);

        m_controlsLayout = new QHBoxLayout();
        m_controlsLayout->setContentsMargins(0, 0, 0, 0);
        m_controlsLayout->setSpacing(0);
        m_controlsLayout->addWidget(m_optionArea, 1);
        m_controlsLayout->addLayout(btnLayout);

        rightLayout->addLayout(m_controlsLayout);

        // Keep internal logic combo boxes hidden but functional
        m_modeCombo = new QComboBox(this);
        m_modeCombo->addItem(tr("文本发送"), QStringLiteral("text"));
        m_modeCombo->addItem(QStringLiteral("HEX 发送"), QStringLiteral("hex"));
        m_modeCombo->hide();
        
        m_lineEndingCombo = new QComboBox(this);
        m_lineEndingCombo->addItem(QStringLiteral("CRLF"), QStringLiteral("crlf"));
        m_lineEndingCombo->hide();
        
        m_byteCountLabel = new QLabel(this);
        m_byteCountLabel->hide();

        rootLayout->addWidget(m_inputEdit, 1);
        rootLayout->addWidget(m_controlPanel);

        connect(sendButton, &QPushButton::clicked, this, &SerialSendPanel::sendRequested);
        connect(clearButton, &QPushButton::clicked, this, &SerialSendPanel::clearDataRequested);

        // Wire up hex check to internal mode combo
        connect(m_hexSendCheckBox, &QPushButton::toggled, this, [this](bool checked) {
            m_modeCombo->setCurrentIndex(checked ? 1 : 0);
        });
        connect(m_hexDisplayCheckBox, &QPushButton::toggled, this, [this](bool checked) {
            setHexDisplayEnabled(checked);
            updateByteCount();
            emit settingsChanged();
        });

        const auto settingHandler = [this]() {
            updateByteCount();
            emit settingsChanged();
        };
        connect(m_inputEdit, &QPlainTextEdit::textChanged, this, settingHandler);
        connect(m_modeCombo, &QComboBox::currentTextChanged, this, [this](const QString &) {
            m_appendNewLineCheckBox->setEnabled(m_modeCombo->currentData().toString() == QStringLiteral("text"));
            m_lineEndingCombo->setEnabled(m_modeCombo->currentData().toString() == QStringLiteral("text"));
            updateByteCount();
            emit settingsChanged();
        });
        connect(m_appendNewLineCheckBox, &QPushButton::toggled, this, settingHandler);
        connect(m_lineEndingCombo, &QComboBox::currentTextChanged, this, settingHandler);
        connect(m_autoSendCheckBox, &QPushButton::toggled, this, settingHandler);
        connect(m_timestampCheckBox, &QPushButton::toggled, this, [this, settingHandler](bool checked) {
            settingHandler();
            emit timestampDisplayChanged(checked);
        });
        connect(m_autoSendSpinBox, &QSpinBox::valueChanged, this, [settingHandler](int) { settingHandler(); });

        updateByteCount();
    }

    void SerialSendPanel::moveControlsTo(QWidget *container)
    {
        if (container == nullptr || m_controlPanel == nullptr || container == m_controlPanel->parentWidget())
        {
            return;
        }

        if (QWidget *oldParent = m_controlPanel->parentWidget(); oldParent != nullptr && oldParent->layout() != nullptr)
        {
            oldParent->layout()->removeWidget(m_controlPanel);
        }

        m_controlPanel->setParent(container);
        if (container->layout() != nullptr)
        {
            container->layout()->addWidget(m_controlPanel);
        }
        m_controlPanel->show();
    }

    void SerialSendPanel::moveOptionsTo(QWidget *container)
    {
        if (container == nullptr || m_optionArea == nullptr || container == m_optionArea->parentWidget())
        {
            return;
        }

        if (m_controlsLayout != nullptr)
        {
            m_controlsLayout->removeWidget(m_optionArea);
        }

        if (QWidget *oldParent = m_optionArea->parentWidget(); oldParent != nullptr && oldParent->layout() != nullptr)
        {
            oldParent->layout()->removeWidget(m_optionArea);
        }

        m_optionArea->setParent(container);
        if (container->layout() != nullptr)
        {
            container->layout()->addWidget(m_optionArea);
        }
        m_optionArea->show();
    }

    QByteArray SerialSendPanel::buildPayload(QString *errorMessage) const
    {
        const QString input = m_inputEdit->toPlainText();
        if (m_modeCombo->currentData().toString() == QStringLiteral("hex"))
        {
            QByteArray bytes;
            if (!DataConvertService::parseHex(input, &bytes, errorMessage))
            {
                return {};
            }
            return bytes;
        }

        QString text = input;
        if (m_hexDisplayCheckBox->isChecked())
        {
            QByteArray bytes;
            if (!DataConvertService::parseHex(text, &bytes, errorMessage))
            {
                return {};
            }
            text = QString::fromUtf8(bytes);
        }
        if (m_appendNewLineCheckBox->isChecked())
        {
            const QString lineEnding = m_lineEndingCombo->currentData().toString();
            if (lineEnding == QStringLiteral("crlf"))
            {
                text.append(QStringLiteral("\r\n"));
            }
            else if (lineEnding == QStringLiteral("lf"))
            {
                text.append(QStringLiteral("\n"));
            }
            else if (lineEnding == QStringLiteral("cr"))
            {
                text.append(QStringLiteral("\r"));
            }
        }
        return text.toUtf8();
    }

    QuickCommandDefinition SerialSendPanel::draftCommand() const
    {
        QuickCommandDefinition command;
        command.content = m_inputEdit->toPlainText();
        command.mode = m_modeCombo->currentData().toString();
        command.appendNewLine = m_appendNewLineCheckBox->isChecked();
        command.lineEnding = m_lineEndingCombo->currentData().toString();
        command.name = command.content.left(12).trimmed();
        if (command.name.isEmpty())
        {
            command.name = tr("未命名命令");
        }
        return command;
    }

    QVariantMap SerialSendPanel::savedSettings() const
    {
        QVariantMap settings;
        settings.insert(QStringLiteral("mode"), m_modeCombo->currentData().toString());
        settings.insert(QStringLiteral("appendNewLine"), m_appendNewLineCheckBox->isChecked());
        settings.insert(QStringLiteral("lineEnding"), m_lineEndingCombo->currentData().toString());
        settings.insert(QStringLiteral("autoSend"), m_autoSendCheckBox->isChecked());
        settings.insert(QStringLiteral("autoSendInterval"), m_autoSendSpinBox->value());
        settings.insert(QStringLiteral("timestamp"), m_timestampCheckBox->isChecked());
        settings.insert(QStringLiteral("hexDisplay"), m_hexDisplayCheckBox->isChecked());
        settings.insert(QStringLiteral("enterSend"), m_enterSendCheckBox->isChecked());
        return settings;
    }

    void SerialSendPanel::applySettings(const QVariantMap &settings)
    {
        const int modeIndex = m_modeCombo->findData(settings.value(QStringLiteral("mode"), QStringLiteral("text")).toString());
        if (modeIndex >= 0)
        {
            m_modeCombo->setCurrentIndex(modeIndex);
        }
        m_appendNewLineCheckBox->setChecked(settings.value(QStringLiteral("appendNewLine"), true).toBool());
        const int endingIndex = m_lineEndingCombo->findData(settings.value(QStringLiteral("lineEnding"), QStringLiteral("crlf")).toString());
        if (endingIndex >= 0)
        {
            m_lineEndingCombo->setCurrentIndex(endingIndex);
        }
        m_autoSendCheckBox->setChecked(settings.value(QStringLiteral("autoSend"), false).toBool());
        m_autoSendSpinBox->setValue(settings.value(QStringLiteral("autoSendInterval"), 1000).toInt());
        m_timestampCheckBox->setChecked(settings.value(QStringLiteral("timestamp"), true).toBool());
        m_hexSendCheckBox->setChecked(m_modeCombo->currentData().toString() == QStringLiteral("hex"));
        m_hexDisplayCheckBox->setChecked(settings.value(QStringLiteral("hexDisplay"), false).toBool());
        m_enterSendCheckBox->setChecked(settings.value(QStringLiteral("enterSend"), false).toBool());
        updateByteCount();
    }

    void SerialSendPanel::setConnected(bool connected)
    {
        Q_UNUSED(connected)
    }

    void SerialSendPanel::setCommandDraft(const QuickCommandDefinition &command)
    {
        m_inputEdit->setPlainText(command.content);
        const int modeIndex = m_modeCombo->findData(command.mode);
        if (modeIndex >= 0)
        {
            m_modeCombo->setCurrentIndex(modeIndex);
        }
        m_hexSendCheckBox->setChecked(m_modeCombo->currentData().toString() == QStringLiteral("hex"));
        // 移除强制覆盖追加新行选项，尊重用户当前的界面选择
        updateByteCount();
    }

    bool SerialSendPanel::autoSendEnabled() const
    {
        return m_autoSendCheckBox->isChecked();
    }

    int SerialSendPanel::autoSendIntervalMs() const
    {
        return m_autoSendSpinBox->value();
    }

    bool SerialSendPanel::timestampEnabled() const
    {
        return m_timestampCheckBox->isChecked();
    }

    bool SerialSendPanel::eventFilter(QObject *watched, QEvent *event)
    {
        if (watched == m_inputEdit
            && event->type() == QEvent::KeyPress
            && m_enterSendCheckBox->isChecked())
        {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            const bool sendKey = keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter;
            const bool keepEditing = keyEvent->modifiers().testFlag(Qt::ShiftModifier);
            if (sendKey && !keepEditing)
            {
                emit sendRequested();
                return true;
            }
        }

        if (event->type() == QEvent::MouseButtonRelease && m_optionCells.contains(qobject_cast<QWidget *>(watched)))
        {
            if (auto *toggle = m_optionCells.value(qobject_cast<QWidget *>(watched)); toggle != nullptr)
            {
                toggle->click();
                return true;
            }
        }

        return QWidget::eventFilter(watched, event);
    }

    void SerialSendPanel::setHexDisplayEnabled(bool enabled)
    {
        if (m_updatingHexDisplay)
        {
            return;
        }

        m_updatingHexDisplay = true;
        const QSignalBlocker blocker(m_inputEdit);

        QString errorMessage;
        const QString input = m_inputEdit->toPlainText();
        if (enabled)
        {
            m_inputEdit->setPlainText(DataConvertService::formatHex(input.toUtf8(), QStringLiteral("space"), true));
        }
        else
        {
            QByteArray bytes;
            if (DataConvertService::parseHex(input, &bytes, &errorMessage))
            {
                m_inputEdit->setPlainText(QString::fromUtf8(bytes));
            }
        }

        m_updatingHexDisplay = false;
    }

    void SerialSendPanel::updateByteCount()
    {
        QString errorMessage;
        const QByteArray payload = buildPayload(&errorMessage);
        if (!errorMessage.isEmpty())
        {
            m_byteCountLabel->setText(tr("当前：HEX 非法"));
            return;
        }
        m_byteCountLabel->setText(tr("当前：%1 B").arg(payload.size()));
    }

} // namespace est
