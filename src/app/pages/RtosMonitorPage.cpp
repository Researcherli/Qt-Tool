#include "pages/RtosMonitorPage.h"

#include "databus/DataBus.h"
#include "databus/DataPacket.h"
#include "plugin/ICore.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QPainter>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace est
{

    // ---------------------------------------------------------------------------
    // 栈配置对话框
    // ---------------------------------------------------------------------------
    namespace
    {

        /// 弹出栈配置对话框，返回 true 表示用户点击了确定
        bool showStackConfigDialog(QWidget *parent,
                                    QHash<QString, int> &config,
                                    const RtosSnapshot &currentSnapshot)
        {
            // 收集所有任务名（配置 + 当前快照）
            QStringList allNames;
            for (auto it = config.cbegin(); it != config.cend(); ++it)
                allNames.append(it.key());
            for (const auto &task : currentSnapshot.tasks)
            {
                if (!allNames.contains(task.name))
                    allNames.append(task.name);
            }
            allNames.removeDuplicates();
            std::sort(allNames.begin(), allNames.end());

            if (allNames.isEmpty())
            {
                QMessageBox::information(parent,
                    RtosMonitorPage::tr("提示"),
                    RtosMonitorPage::tr("当前没有任务数据，请先通过串口接收 vTaskList 输出。"));
                return false;
            }

            QDialog dlg(parent);
            dlg.setWindowTitle(RtosMonitorPage::tr("任务栈配置"));
            dlg.setMinimumWidth(400);

            auto *layout = new QVBoxLayout(&dlg);

            auto *infoLabel = new QLabel(
                RtosMonitorPage::tr("为每个任务设置总栈大小（字），配置后将显示栈使用百分比。"), &dlg);
            infoLabel->setWordWrap(true);
            layout->addWidget(infoLabel);

            auto *formLayout = new QFormLayout();
            QVector<QSpinBox *> spinBoxes;

            for (const auto &name : allNames)
            {
                auto *spin = new QSpinBox(&dlg);
                spin->setRange(0, 65535);
                spin->setSuffix(QStringLiteral(" 字"));
                spin->setSpecialValueText(RtosMonitorPage::tr("未配置"));
                spin->setValue(config.value(name, 0));
                formLayout->addRow(name, spin);
                spinBoxes.append(spin);
            }

            layout->addLayout(formLayout);

            auto *buttons = new QDialogButtonBox(
                QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
            layout->addWidget(buttons);

            QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
            QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

            if (dlg.exec() != QDialog::Accepted)
                return false;

            // 读取配置
            for (int i = 0; i < allNames.size(); ++i)
            {
                const int val = spinBoxes[i]->value();
                if (val > 0)
                    config[allNames[i]] = val;
                else
                    config.remove(allNames[i]);
            }

            return true;
        }

        double cpuPercentForTask(const RtosSnapshot &snapshot, const QString &taskName)
        {
            for (const auto &cpu : snapshot.cpuStats)
            {
                if (cpu.name == taskName)
                    return cpu.cpuPercent;
            }
            return -1.0;
        }

        /// 绘制 CPU 概览的自定义 widget
        class CpuBarWidget : public QWidget
        {
        public:
            explicit CpuBarWidget(QWidget *parent = nullptr)
                : QWidget(parent)
            {
            }

            struct BarData
            {
                QString name;
                double percent = 0.0;
                QColor color;
            };

            void setBars(const QVector<BarData> &bars)
            {
                m_bars = bars;
                update();
            }

        protected:
            void paintEvent(QPaintEvent *event) override
            {
                Q_UNUSED(event)
                QPainter p(this);
                p.setRenderHint(QPainter::Antialiasing);

                const int margin = 8;
                const int barHeight = 20;
                const int spacing = 4;
                const int labelWidth = 120;
                const int percentWidth = 50;
                const int totalHeight = margin * 2 + m_bars.size() * (barHeight + spacing);

                setMinimumHeight(totalHeight);
                setMaximumHeight(totalHeight);

                const int availWidth = width() - margin * 2 - labelWidth - percentWidth - 8;
                const int barStartX = margin + labelWidth;

                for (int i = 0; i < m_bars.size(); ++i)
                {
                    const int y = margin + i * (barHeight + spacing);
                    const auto &bar = m_bars[i];

                    // 任务名
                    p.setPen(QColor(QStringLiteral("#333333")));
                    p.drawText(QRect(margin, y, labelWidth - 4, barHeight),
                               Qt::AlignVCenter | Qt::AlignRight, bar.name);

                    // 进度条背景
                    const int barW = static_cast<int>(availWidth * qMin(bar.percent, 100.0) / 100.0);
                    const QColor bgColor(QStringLiteral("#E0E0E0"));

                    p.setPen(Qt::NoPen);
                    p.setBrush(bgColor);
                    p.drawRoundedRect(barStartX, y + 2, availWidth, barHeight - 4, 3, 3);

                    // 进度条填充
                    if (barW > 0)
                    {
                        p.setBrush(bar.color);
                        p.drawRoundedRect(barStartX, y + 2, qMax(barW, 6), barHeight - 4, 3, 3);
                    }

                    // 百分比文字
                    p.setPen(QColor(QStringLiteral("#333333")));
                    p.drawText(QRect(barStartX + availWidth + 4, y, percentWidth, barHeight),
                               Qt::AlignVCenter | Qt::AlignLeft,
                               QStringLiteral("%1%").arg(bar.percent, 0, 'f', 1));
                }
            }

        private:
            QVector<BarData> m_bars;
        };

    } // namespace

    // ---------------------------------------------------------------------------
    // RtosMonitorPage
    // ---------------------------------------------------------------------------

    RtosMonitorPage::RtosMonitorPage(ICore *core, QWidget *parent)
        : QWidget(parent)
        , m_dataBus(core ? core->dataBus() : nullptr)
    {
        setObjectName(QStringLiteral("rtosMonitorPage"));

        // 从 QSettings 加载栈配置
        QSettings settings(QStringLiteral("EST"), QStringLiteral("RtosMonitor"));
        const QVariantMap saved = settings.value(QStringLiteral("stackConfig")).toMap();
        for (auto it = saved.cbegin(); it != saved.cend(); ++it)
            m_stackConfig.insert(it.key(), it.value().toInt());

        auto *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(8, 8, 8, 8);
        rootLayout->setSpacing(8);

        setupToolbar(rootLayout);
        setupTaskTable(rootLayout);
        setupCpuOverview(rootLayout);

        subscribeDataBus();
    }

    RtosMonitorPage::~RtosMonitorPage()
    {
        if (m_subscription.isValid() && m_dataBus)
            m_dataBus->unsubscribe(m_subscription);

        // 持久化栈配置
        QSettings settings(QStringLiteral("EST"), QStringLiteral("RtosMonitor"));
        QVariantMap saved;
        for (auto it = m_stackConfig.cbegin(); it != m_stackConfig.cend(); ++it)
            saved.insert(it.key(), it.value());
        settings.setValue(QStringLiteral("stackConfig"), saved);
    }

    // ---------------------------------------------------------------------------
    // UI 构建
    // ---------------------------------------------------------------------------

    void RtosMonitorPage::setupToolbar(QVBoxLayout *rootLayout)
    {
        auto *toolbar = new QWidget(this);
        toolbar->setObjectName(QStringLiteral("rtosToolbar"));
        auto *layout = new QHBoxLayout(toolbar);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);

        // 格式选择
        auto *fmtLabel = new QLabel(tr("格式:"), toolbar);
        layout->addWidget(fmtLabel);

        m_formatCombo = new QComboBox(toolbar);
        m_formatCombo->addItem(tr("自动检测"));
        m_formatCombo->addItem(QStringLiteral("vTaskList"));
        m_formatCombo->addItem(QStringLiteral("vTaskGetRunTimeStats"));
        m_formatCombo->setFixedWidth(160);
        layout->addWidget(m_formatCombo);

        layout->addSpacing(8);

        // 暂停
        m_pauseBtn = new QToolButton(toolbar);
        m_pauseBtn->setText(tr("⏸ 暂停"));
        m_pauseBtn->setCheckable(true);
        layout->addWidget(m_pauseBtn);

        // 清空
        m_clearBtn = new QPushButton(tr("清空"), toolbar);
        layout->addWidget(m_clearBtn);

        // 立即解析
        m_forceParseBtn = new QPushButton(tr("立即解析"), toolbar);
        m_forceParseBtn->setToolTip(tr("强制解析当前行缓存，或从剪贴板粘贴数据"));
        layout->addWidget(m_forceParseBtn);

        layout->addSpacing(8);

        // 配置总栈
        m_configStackBtn = new QPushButton(tr("配置总栈"), toolbar);
        layout->addWidget(m_configStackBtn);

        layout->addStretch(1);

        // 状态标签
        m_statusLabel = new QLabel(tr("等待数据..."), toolbar);
        m_statusLabel->setObjectName(QStringLiteral("rtosStatusLabel"));
        layout->addWidget(m_statusLabel);

        rootLayout->addWidget(toolbar);

        // 连接信号
        connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &RtosMonitorPage::onFormatChanged);
        connect(m_pauseBtn, &QToolButton::toggled,
                this, &RtosMonitorPage::onTogglePause);
        connect(m_clearBtn, &QPushButton::clicked,
                this, &RtosMonitorPage::onClear);
        connect(m_forceParseBtn, &QPushButton::clicked,
                this, &RtosMonitorPage::onForceParse);
        connect(m_configStackBtn, &QPushButton::clicked,
                this, &RtosMonitorPage::onConfigureStack);
    }

    void RtosMonitorPage::setupTaskTable(QVBoxLayout *rootLayout)
    {
        m_taskTable = new QTableWidget(this);
        m_taskTable->setObjectName(QStringLiteral("rtosTaskTable"));
        m_taskTable->setColumnCount(ColumnCount);

        m_taskTable->setHorizontalHeaderLabels({
            tr("#"),          // ColNumber
            tr("任务名"),      // ColName
            tr("状态"),       // ColState
            tr("优先级"),     // ColPriority
            tr("栈余量(字)"), // ColStackRemain
            tr("栈使用%"),    // ColStackPercent
            tr("CPU%"),       // ColCpuPercent
        });

        m_taskTable->horizontalHeader()->setSectionResizeMode(ColNumber, QHeaderView::ResizeToContents);
        m_taskTable->horizontalHeader()->setSectionResizeMode(ColName, QHeaderView::Stretch);
        m_taskTable->horizontalHeader()->setSectionResizeMode(ColState, QHeaderView::ResizeToContents);
        m_taskTable->horizontalHeader()->setSectionResizeMode(ColPriority, QHeaderView::ResizeToContents);
        m_taskTable->horizontalHeader()->setSectionResizeMode(ColStackRemain, QHeaderView::ResizeToContents);
        m_taskTable->horizontalHeader()->setSectionResizeMode(ColStackPercent, QHeaderView::Stretch);
        m_taskTable->horizontalHeader()->setSectionResizeMode(ColCpuPercent, QHeaderView::Fixed);
        m_taskTable->horizontalHeader()->resizeSection(ColCpuPercent, 100);

        m_taskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_taskTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_taskTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_taskTable->setAlternatingRowColors(true);
        m_taskTable->verticalHeader()->setVisible(false);

        rootLayout->addWidget(m_taskTable, 3);
    }

    void RtosMonitorPage::setupCpuOverview(QVBoxLayout *rootLayout)
    {
        m_cpuOverview = new CpuBarWidget(this);
        m_cpuOverview->setObjectName(QStringLiteral("rtosCpuOverview"));
        m_cpuOverview->setMinimumHeight(60);

        auto *sectionLabel = new QLabel(
            QStringLiteral("<b>%1</b>").arg(tr("CPU 占比")), this);

        rootLayout->addWidget(sectionLabel);
        rootLayout->addWidget(m_cpuOverview, 1);
    }

    // ---------------------------------------------------------------------------
    // DataBus
    // ---------------------------------------------------------------------------

    void RtosMonitorPage::subscribeDataBus()
    {
        if (m_dataBus == nullptr)
            return;

        m_subscription = m_dataBus->subscribe(
            QStringLiteral("transport.serial.*"),
            [this](const DataPacket &packet)
            {
                if (packet.metadata.value(QStringLiteral("direction")).toString() == QStringLiteral("tx"))
                    return;

                QPointer<RtosMonitorPage> self(this);
                QMetaObject::invokeMethod(this, [self, packet]() {
                    if (self)
                        self->handleSerialPacket(packet);
                }, Qt::QueuedConnection);
            });
    }

    // ---------------------------------------------------------------------------
    // 解析和更新
    // ---------------------------------------------------------------------------

    void RtosMonitorPage::handleSerialPacket(const DataPacket &packet)
    {
        if (m_paused || packet.rawPayload.isEmpty())
            return;

        const QString text = QString::fromUtf8(packet.rawPayload);
        if (text.trimmed().isEmpty())
            return;

        m_lineBuffer += text;
        if (!m_lineBuffer.endsWith(QLatin1Char('\n')))
            m_lineBuffer += QLatin1Char('\n');

        if (m_lineBuffer.size() > 65536)
            m_lineBuffer = m_lineBuffer.right(32768);

        tryAutoParse();
    }

    RtosSnapshot RtosMonitorPage::mergedSnapshot(const RtosSnapshot &incoming) const
    {
        RtosSnapshot merged = incoming;

        if (merged.tasks.isEmpty() && !m_lastSnapshot.tasks.isEmpty())
            merged.tasks = m_lastSnapshot.tasks;
        if (merged.cpuStats.isEmpty() && !m_lastSnapshot.cpuStats.isEmpty())
            merged.cpuStats = m_lastSnapshot.cpuStats;

        merged.mergeCpuStats();
        return merged;
    }

    void RtosMonitorPage::tryAutoParse()
    {
        if (m_paused)
            return;

        // 行缓存至少要包含几条数据行才尝试解析
        const int lineCount = m_lineBuffer.count(QChar::LineFeed);
        if (lineCount < 3)
            return;

        // 检查格式选择：0=自动, 1=vTaskList, 2=runtime stats
        const int fmtIdx = m_formatCombo->currentIndex();

        RtosSnapshot snapshot;

        if (fmtIdx == 0)
        {
            // 自动检测
            snapshot = RtosTaskParser::parseSnapshot(
                m_lineBuffer, QDateTime::currentMSecsSinceEpoch());
        }
        else
        {
            // 指定格式
            snapshot.timestamp = QDateTime::currentMSecsSinceEpoch();
            if (fmtIdx == 1)
                snapshot.tasks = RtosTaskParser::parseTaskList(m_lineBuffer);
            else if (fmtIdx == 2)
                snapshot.cpuStats = RtosTaskParser::parseRuntimeStats(m_lineBuffer);
        }

        if (snapshot.isEmpty())
            return;

        updateFromSnapshot(mergedSnapshot(snapshot));
        m_lineBuffer.clear();
    }

    void RtosMonitorPage::updateFromSnapshot(const RtosSnapshot &snapshot)
    {
        m_lastSnapshot = snapshot;

        m_taskTable->setUpdatesEnabled(false);
        m_taskTable->setRowCount(0);

        const int taskCount = snapshot.tasks.size();

        for (int row = 0; row < taskCount; ++row)
        {
            const auto &task = snapshot.tasks[row];

            m_taskTable->insertRow(row);

            // # (列0)
            auto *numItem = new QTableWidgetItem(QString::number(task.taskNumber));
            numItem->setTextAlignment(Qt::AlignCenter);
            m_taskTable->setItem(row, ColNumber, numItem);

            // 任务名 (列1)
            auto *nameItem = new QTableWidgetItem(task.name);
            m_taskTable->setItem(row, ColName, nameItem);

            // 状态 (列2) — 带颜色
            const QColor stateColor = RtosTaskParser::stateToColor(task.state);
            const QString stateText = RtosTaskParser::stateToSymbol(task.state);
            auto *stateItem = new QTableWidgetItem(stateText);
            stateItem->setForeground(stateColor);
            stateItem->setData(Qt::DecorationRole,
                               QVariant::fromValue<QColor>(stateColor));
            Q_UNUSED(stateColor)
            m_taskTable->setItem(row, ColState, stateItem);

            // 优先级 (列3)
            auto *prioItem = new QTableWidgetItem(QString::number(task.priority));
            prioItem->setTextAlignment(Qt::AlignCenter);
            m_taskTable->setItem(row, ColPriority, prioItem);

            // 栈余量 (列4)
            auto *stackItem = new QTableWidgetItem(
                QStringLiteral("%1").arg(task.stackRemaining));
            stackItem->setTextAlignment(Qt::AlignCenter);
            m_taskTable->setItem(row, ColStackRemain, stackItem);

            // 栈使用% (列5) — QProgressBar
            const int totalStack = totalStackForTask(task.name);
            if (totalStack > 0)
            {
                const int used = qBound(0, totalStack - task.stackRemaining, totalStack);
                const int percent = (used * 100) / totalStack;

                auto *progressBar = new QProgressBar();
                progressBar->setRange(0, 100);
                progressBar->setValue(percent);
                progressBar->setTextVisible(true);
                progressBar->setFormat(QStringLiteral("%1%").arg(percent));
                progressBar->setFixedHeight(20);

                // 根据使用率着色
                if (percent > 85)
                    progressBar->setStyleSheet(QStringLiteral(
                        "QProgressBar::chunk { background-color: #F44336; }"));
                else if (percent > 60)
                    progressBar->setStyleSheet(QStringLiteral(
                        "QProgressBar::chunk { background-color: #FF9800; }"));
                else
                    progressBar->setStyleSheet(QStringLiteral(
                        "QProgressBar::chunk { background-color: #4CAF50; }"));

                m_taskTable->setCellWidget(row, ColStackPercent, progressBar);
            }
            else
            {
                auto *naItem = new QTableWidgetItem(QStringLiteral("--"));
                naItem->setTextAlignment(Qt::AlignCenter);
                naItem->setForeground(QColor(QStringLiteral("#9E9E9E")));
                m_taskTable->setItem(row, ColStackPercent, naItem);
            }

            // CPU% (列6) — 从 cpuStats 中按名称匹配
            auto *cpuItem = new QTableWidgetItem(QStringLiteral("--"));
            cpuItem->setTextAlignment(Qt::AlignCenter);
            const double cpuPercent = task.cpuPercent >= 0.0
                ? task.cpuPercent
                : cpuPercentForTask(snapshot, task.name);
            if (cpuPercent >= 0.0)
            {
                cpuItem->setText(
                    QStringLiteral("%1%").arg(cpuPercent, 0, 'f', 1));
            }
            m_taskTable->setItem(row, ColCpuPercent, cpuItem);

            // 行着色（按状态）
            const QColor tint = rowTintForState(task.state);
            for (int col = 0; col < ColumnCount; ++col)
            {
                QTableWidgetItem *item = m_taskTable->item(row, col);
                if (item && tint.isValid())
                    item->setBackground(tint);
            }
        }

        m_taskTable->setUpdatesEnabled(true);

        // 更新 CPU 概览
        updateCpuOverview(snapshot);
        updateStatusLabel(snapshot);
    }

    void RtosMonitorPage::updateStatusLabel(const RtosSnapshot &snapshot)
    {
        const QString timeStr = snapshot.timestamp > 0
            ? QDateTime::fromMSecsSinceEpoch(snapshot.timestamp)
                  .toString(QStringLiteral("HH:mm:ss"))
            : QStringLiteral("--:--:--");

        int runningCount = 0, readyCount = 0, blockedCount = 0, suspendedCount = 0, deletedCount = 0;
        for (const auto &task : snapshot.tasks)
        {
            switch (task.state)
            {
            case RtosTaskState::Running:
                ++runningCount;
                break;
            case RtosTaskState::Ready:
                ++readyCount;
                break;
            case RtosTaskState::Blocked:
                ++blockedCount;
                break;
            case RtosTaskState::Suspended:
                ++suspendedCount;
                break;
            case RtosTaskState::Deleted:
                ++deletedCount;
                break;
            default:
                break;
            }
        }

        QStringList parts;
        parts << tr("上次更新: %1").arg(timeStr)
              << tr("共 %1 个任务").arg(snapshot.taskCount());
        if (runningCount > 0)
            parts << tr("运行 %1").arg(runningCount);
        if (readyCount > 0)
            parts << tr("就绪 %1").arg(readyCount);
        if (blockedCount > 0)
            parts << tr("阻塞 %1").arg(blockedCount);
        if (suspendedCount > 0)
            parts << tr("挂起 %1").arg(suspendedCount);
        if (deletedCount > 0)
            parts << tr("删除 %1").arg(deletedCount);

        m_statusLabel->setText(parts.join(tr(" │ ")));
    }

    void RtosMonitorPage::updateCpuOverview(const RtosSnapshot &snapshot)
    {
        auto *widget = static_cast<CpuBarWidget *>(m_cpuOverview);
        if (!widget)
            return;

        // 如果有 CPU stats 数据，直接使用
        if (!snapshot.cpuStats.isEmpty())
        {
            QVector<CpuBarWidget::BarData> bars;
            for (const auto &cpu : snapshot.cpuStats)
            {
                CpuBarWidget::BarData bar;
                bar.name = cpu.name;
                bar.percent = cpu.cpuPercent;

                // 从任务列表中找对应的 state 来取颜色
                bar.color = QColor(QStringLiteral("#2196F3")); // 默认蓝色
                for (const auto &task : snapshot.tasks)
                {
                    if (task.name == cpu.name)
                    {
                        bar.color = RtosTaskParser::stateToColor(task.state);
                        break;
                    }
                }
                bars.append(bar);
            }
            widget->setBars(bars);
        }
        else if (!snapshot.tasks.isEmpty())
        {
            // 没有 CPU stats，按任务编号均分展示（仅占位）
            QVector<CpuBarWidget::BarData> bars;
            for (const auto &task : snapshot.tasks)
            {
                CpuBarWidget::BarData bar;
                bar.name = task.name;
                bar.percent = 0.0;
                bar.color = RtosTaskParser::stateToColor(task.state);
                bars.append(bar);
            }
            widget->setBars(bars);
        }
        else
        {
            widget->setBars({});
        }
    }

    // ---------------------------------------------------------------------------
    // 辅助方法
    // ---------------------------------------------------------------------------

    int RtosMonitorPage::totalStackForTask(const QString &taskName) const
    {
        return m_stackConfig.value(taskName, 0);
    }

    QColor RtosMonitorPage::rowTintForState(RtosTaskState state) const
    {
        switch (state)
        {
        case RtosTaskState::Running:
            return QColor(QStringLiteral("#E0F7F7"));
        case RtosTaskState::Ready:
            return QColor(QStringLiteral("#E8F5E9")); // 浅绿
        case RtosTaskState::Blocked:
            return QColor(QStringLiteral("#E3F2FD")); // 浅蓝
        case RtosTaskState::Suspended:
            return QColor(QStringLiteral("#F5F5F5")); // 浅灰
        case RtosTaskState::Deleted:
            return QColor(QStringLiteral("#FFEBEE")); // 浅红
        default:
            return QColor();
        }
    }

    // ---------------------------------------------------------------------------
    // Slot 实现
    // ---------------------------------------------------------------------------

    void RtosMonitorPage::onFormatChanged(int /*index*/)
    {
        // 格式切换后不清除已有数据，下次自动解析用新格式
    }

    void RtosMonitorPage::onTogglePause()
    {
        m_paused = m_pauseBtn->isChecked();
        m_pauseBtn->setText(m_paused ? tr("▶ 继续") : tr("⏸ 暂停"));
        emit statusMessageGenerated(
            m_paused ? tr("RTOS 监视已暂停") : tr("RTOS 监视已继续"));
    }

    void RtosMonitorPage::onClear()
    {
        m_lineBuffer.clear();
        m_taskTable->setRowCount(0);
        m_lastSnapshot = RtosSnapshot();

        auto *widget = static_cast<CpuBarWidget *>(m_cpuOverview);
        if (widget)
            widget->setBars({});

        m_statusLabel->setText(tr("已清空"));
        emit statusMessageGenerated(tr("RTOS 监视数据已清空"));
    }

    void RtosMonitorPage::onForceParse()
    {
        // 如果行缓存为空，尝试从剪贴板读取
        QString text = m_lineBuffer.trimmed();
        if (text.isEmpty())
        {
            text = QApplication::clipboard()->text().trimmed();
            if (text.isEmpty())
            {
                QMessageBox::information(this, tr("提示"),
                    tr("行缓存和剪贴板均为空。\n请先在串口工具中发送 vTaskList 或 vTaskGetRunTimeStats 命令。"));
                return;
            }
        }

        const int fmtIdx = m_formatCombo->currentIndex();
        RtosSnapshot snapshot;
        snapshot.timestamp = QDateTime::currentMSecsSinceEpoch();

        if (fmtIdx == 0)
            snapshot = RtosTaskParser::parseSnapshot(text, snapshot.timestamp);
        else if (fmtIdx == 1)
            snapshot.tasks = RtosTaskParser::parseTaskList(text);
        else if (fmtIdx == 2)
            snapshot.cpuStats = RtosTaskParser::parseRuntimeStats(text);

        if (snapshot.isEmpty())
        {
            QMessageBox::warning(this, tr("解析失败"),
                tr("无法从当前数据中解析出有效任务信息。\n请检查数据格式是否匹配所选的解析模式。"));
            return;
        }

        snapshot = mergedSnapshot(snapshot);
        updateFromSnapshot(snapshot);
        m_lineBuffer.clear();

        emit statusMessageGenerated(
            tr("手动解析成功：共 %1 个任务").arg(snapshot.taskCount()));
    }

    void RtosMonitorPage::onConfigureStack()
    {
        if (showStackConfigDialog(this, m_stackConfig, m_lastSnapshot))
        {
            // 重新刷新表格显示栈百分比
            if (!m_lastSnapshot.isEmpty())
                updateFromSnapshot(m_lastSnapshot);

            emit statusMessageGenerated(tr("栈配置已更新"));
        }
    }

} // namespace est
