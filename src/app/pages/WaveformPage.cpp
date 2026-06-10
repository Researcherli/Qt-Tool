#include "pages/WaveformPage.h"

#include "databus/DataBus.h"
#include "databus/DataPacket.h"
#include "databus/SubscriptionHandle.h"
#include "plugin/ICore.h"
#include "widgets/WaveformChartWidget.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace est
{

    // ---------------------------------------------------------------------------
    // 添加序列对话框
    // ---------------------------------------------------------------------------
    namespace
    {

        /// 预设解析器列表
        struct ParserPreset
        {
            QString label;
            QString pattern;
        };

        const QVector<ParserPreset> kParserPresets = {
            { QStringLiteral("temp=25.3 格式 (temp=数值)"),
              QStringLiteral("temp\\s*=\\s*(-?\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?)") },
            { QStringLiteral("val:123 格式 (val[:=]数值)"),
              QStringLiteral("val[=:]\\s*(-?\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?)") },
            { QStringLiteral("第一个浮点数"),
              QStringLiteral("__first_float__") },
            { QStringLiteral("第一个整数"),
              QStringLiteral("__first_int__") },
            { QStringLiteral("原始字节 Byte 0 (无符号)"),
              QStringLiteral("__hex_byte_0__") },
            { QStringLiteral("原始字节 Byte 1 (无符号)"),
              QStringLiteral("__hex_byte_1__") },
            { QStringLiteral("原始字节 Word LE (16位)"),
              QStringLiteral("__hex_word_le__") },
            { QStringLiteral("自定义正则..."),
              QStringLiteral("") },
        };

        const QVector<QColor> kColorOptions = {
            QColor(QStringLiteral("#4CAF50")),   // 绿
            QColor(QStringLiteral("#2196F3")),   // 蓝
            QColor(QStringLiteral("#FF9800")),   // 橙
            QColor(QStringLiteral("#E91E63")),   // 粉
            QColor(QStringLiteral("#9C27B0")),   // 紫
            QColor(QStringLiteral("#00BCD4")),   // 青
            QColor(QStringLiteral("#F44336")),   // 红
            QColor(QStringLiteral("#FFEB3B")),   // 黄
        };

        bool showAddSeriesDialog(QWidget *parent,
                                  QString &outName,
                                  QString &outPattern,
                                  QColor &outColor)
        {
            QDialog dlg(parent);
            dlg.setWindowTitle(WaveformPage::tr("添加数据序列"));
            dlg.setMinimumWidth(420);

            auto *form = new QFormLayout(&dlg);

            // 名称
            auto *nameEdit = new QLineEdit(&dlg);
            nameEdit->setPlaceholderText(WaveformPage::tr("如: 温度、电压、PWM..."));
            form->addRow(WaveformPage::tr("序列名称"), nameEdit);

            // 解析器预设
            auto *presetCombo = new QComboBox(&dlg);
            for (const auto &preset : kParserPresets)
                presetCombo->addItem(preset.label);
            form->addRow(WaveformPage::tr("解析规则"), presetCombo);

            // 自定义正则（高级模式）
            auto *customPatternEdit = new QLineEdit(&dlg);
            customPatternEdit->setPlaceholderText(WaveformPage::tr("输入自定义正则，捕获组(...)提取数值"));
            customPatternEdit->setEnabled(false);
            form->addRow(WaveformPage::tr("自定义正则"), customPatternEdit);

            QObject::connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    [presetCombo, customPatternEdit](int index)
                    {
                        const bool isCustom = (index == kParserPresets.size() - 1);
                        customPatternEdit->setEnabled(isCustom);
                        if (!isCustom)
                            customPatternEdit->setText(kParserPresets[index].pattern);
                    });
            presetCombo->setCurrentIndex(0);
            customPatternEdit->setText(kParserPresets.first().pattern);

            // 颜色选择
            auto *colorCombo = new QComboBox(&dlg);
            for (const auto &c : kColorOptions)
            {
                QPixmap px(24, 24);
                px.fill(c);
                colorCombo->addItem(QIcon(px), QString());
            }
            form->addRow(WaveformPage::tr("线条颜色"), colorCombo);

            // 确定 / 取消
            auto *buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
            form->addRow(buttons);

            QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

            if (dlg.exec() != QDialog::Accepted)
                return false;

            outName = nameEdit->text().trimmed();
            if (outName.isEmpty())
                outName = QStringLiteral("序列 %1").arg(presetCombo->currentIndex() + 1);

            const int presetIdx = presetCombo->currentIndex();
            if (presetIdx == kParserPresets.size() - 1)
                outPattern = customPatternEdit->text().trimmed();
            else
                outPattern = kParserPresets[presetIdx].pattern;

            if (outPattern.isEmpty())
            {
                QMessageBox::warning(parent,
                                     WaveformPage::tr("无法添加序列"),
                                     WaveformPage::tr("解析规则不能为空。"));
                return false;
            }

            const int colorIdx = colorCombo->currentIndex();
            outColor = (colorIdx >= 0 && colorIdx < kColorOptions.size())
                           ? kColorOptions[colorIdx]
                           : QColor();

            return true;
        }

    } // namespace

    // ---------------------------------------------------------------------------
    // WaveformPage
    // ---------------------------------------------------------------------------

    WaveformPage::WaveformPage(ICore *core, QWidget *parent)
        : QWidget(parent)
        , m_dataBus(core ? core->dataBus() : nullptr)
    {
        setObjectName(QStringLiteral("waveformPage"));

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(8, 8, 8, 8);
        rootLayout->setSpacing(8);

        setupToolbar();

        m_chartWidget = new WaveformChartWidget(this);
        rootLayout->addWidget(m_chartWidget, 1);

        subscribeDataBus();
    }

    WaveformPage::~WaveformPage()
    {
        if (m_subscription.isValid() && m_dataBus)
            m_dataBus->unsubscribe(m_subscription);
    }

    void WaveformPage::setupToolbar()
    {
        auto *toolbar = new QWidget(this);
        toolbar->setObjectName(QStringLiteral("waveformToolbar"));
        auto *toolbarLayout = new QHBoxLayout(toolbar);
        toolbarLayout->setContentsMargins(0, 0, 0, 0);
        toolbarLayout->setSpacing(6);

        // 添加序列
        m_addSeriesBtn = new QPushButton(tr("添加序列"), toolbar);
        m_addSeriesBtn->setObjectName(QStringLiteral("wfAddSeriesBtn"));
        toolbarLayout->addWidget(m_addSeriesBtn);

        // 删除序列
        m_removeSeriesBtn = new QPushButton(tr("删除序列"), toolbar);
        m_removeSeriesBtn->setObjectName(QStringLiteral("wfRemoveSeriesBtn"));
        m_removeSeriesBtn->setEnabled(false);
        toolbarLayout->addWidget(m_removeSeriesBtn);

        toolbarLayout->addSpacing(12);

        // 暂停 / 继续
        m_pauseBtn = new QToolButton(toolbar);
        m_pauseBtn->setObjectName(QStringLiteral("wfPauseBtn"));
        m_pauseBtn->setText(tr("暂停"));
        m_pauseBtn->setCheckable(true);
        m_pauseBtn->setChecked(false);
        toolbarLayout->addWidget(m_pauseBtn);

        // 清空
        m_clearBtn = new QPushButton(tr("清空"), toolbar);
        m_clearBtn->setObjectName(QStringLiteral("wfClearBtn"));
        toolbarLayout->addWidget(m_clearBtn);

        toolbarLayout->addSpacing(12);

        // 时间窗口标签
        auto *twLabel = new QLabel(tr("时间窗口:"), toolbar);
        toolbarLayout->addWidget(twLabel);

        m_timeWindowSpin = new QDoubleSpinBox(toolbar);
        m_timeWindowSpin->setObjectName(QStringLiteral("wfTimeWindowSpin"));
        m_timeWindowSpin->setRange(1.0, 300.0);
        m_timeWindowSpin->setSingleStep(1.0);
        m_timeWindowSpin->setValue(10.0);
        m_timeWindowSpin->setSuffix(tr(" 秒"));
        m_timeWindowSpin->setFixedWidth(110);
        toolbarLayout->addWidget(m_timeWindowSpin);

        toolbarLayout->addStretch(1);

        // 状态信息
        m_statusLabel = new QLabel(tr("序列: 0 | 数据包: 0 | 点: 0"), toolbar);
        m_statusLabel->setObjectName(QStringLiteral("wfStatusLabel"));
        toolbarLayout->addWidget(m_statusLabel);

        auto *rootLayout = qobject_cast<QVBoxLayout *>(layout());
        if (rootLayout)
            rootLayout->addWidget(toolbar);

        // 连接信号
        connect(m_addSeriesBtn, &QPushButton::clicked,
                this, &WaveformPage::onAddSeries);
        connect(m_removeSeriesBtn, &QPushButton::clicked,
                this, &WaveformPage::onRemoveSeries);
        connect(m_pauseBtn, &QToolButton::toggled,
                this, &WaveformPage::onTogglePause);
        connect(m_clearBtn, &QPushButton::clicked,
                this, &WaveformPage::onClear);
        connect(m_timeWindowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &WaveformPage::onTimeWindowChanged);
    }

    void WaveformPage::subscribeDataBus()
    {
        if (m_dataBus == nullptr)
            return;

        // 订阅所有 transport.serial.* 通道
        m_subscription = m_dataBus->subscribe(QStringLiteral("transport.serial.*"),
            [this](const DataPacket &packet)
            {
                if (packet.metadata.value(QStringLiteral("direction")).toString() == QStringLiteral("tx"))
                    return;

                QMetaObject::invokeMethod(this, [this, packet]() {
                    if (m_chartWidget == nullptr)
                        return;

                    ++m_receivedPackets;
                    m_chartWidget->feedData(
                        packet.channel,
                        packet.timestamp > 0 ? packet.timestamp
                                              : QDateTime::currentMSecsSinceEpoch(),
                        packet.rawPayload);
                    updateStatusText();
                }, Qt::QueuedConnection);
            });
    }

    void WaveformPage::onAddSeries()
    {
        QString name;
        QString pattern;
        QColor color;

        if (!showAddSeriesDialog(this, name, pattern, color))
            return;

        QString errorMessage;
        const int idx = m_chartWidget->addSeries(name, color, pattern, &errorMessage);
        if (idx < 0)
        {
            QMessageBox::warning(this, tr("无法添加序列"), errorMessage);
            return;
        }
        m_removeSeriesBtn->setEnabled(true);
        m_selectedSeriesIndex = idx;
        updateStatusText();

        emit statusMessageGenerated(
            tr("已添加波形序列: %1").arg(name));
    }

    void WaveformPage::onRemoveSeries()
    {
        const int count = m_chartWidget->seriesCount();
        if (count == 0)
        {
            m_removeSeriesBtn->setEnabled(false);
            return;
        }

        // 移除最后一个序列（简化交互）
        const int idx = count - 1;
        const QString seriesName = m_chartWidget->seriesConfig(idx).name;
        m_chartWidget->removeSeries(idx);
        m_selectedSeriesIndex = m_chartWidget->seriesCount() - 1;
        updateStatusText();

        emit statusMessageGenerated(
            tr("已删除波形序列: %1").arg(seriesName));

        if (m_chartWidget->seriesCount() == 0)
            m_removeSeriesBtn->setEnabled(false);
    }

    void WaveformPage::onTogglePause()
    {
        const bool pausing = m_pauseBtn->isChecked();
        m_chartWidget->setPaused(pausing);
        m_pauseBtn->setText(pausing ? tr("继续") : tr("暂停"));

        emit statusMessageGenerated(
            pausing ? tr("波形已暂停") : tr("波形已继续"));
    }

    void WaveformPage::onClear()
    {
        m_chartWidget->clearAll();
        m_receivedPackets = 0;
        updateStatusText();
        emit statusMessageGenerated(tr("波形已清空"));
    }

    void WaveformPage::onTimeWindowChanged(double seconds)
    {
        m_chartWidget->setTimeWindow(seconds);
        updateStatusText();
    }

    void WaveformPage::updateStatusText()
    {
        if (m_statusLabel == nullptr || m_chartWidget == nullptr)
            return;

        m_statusLabel->setText(tr("序列: %1 | 数据包: %2 | 点: %3")
                                   .arg(m_chartWidget->seriesCount())
                                   .arg(m_receivedPackets)
                                   .arg(m_chartWidget->totalPointCount()));
    }

} // namespace est
