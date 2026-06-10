#ifndef EST_RTOSMONITORPAGE_H
#define EST_RTOSMONITORPAGE_H

#include <QWidget>

#include "databus/SubscriptionHandle.h"
#include "services/RtosTaskParser.h"

#include <QHash>
#include <QVBoxLayout>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QToolButton;
class QVBoxLayout;

namespace est
{

    class DataBus;
    class ICore;
    struct DataPacket;

    /**
     * RTOS 任务监视器页面 — 订阅串口数据流，实时解析并可视化 FreeRTOS
     * vTaskList() / vTaskGetRunTimeStats() 输出。
     *
     * 显示内容：
     *   - 任务列表表格（状态、优先级、栈使用、CPU%）
     *   - CPU 占比概览（水平进度条）
     *   - 栈配置对话框（手动录入总栈大小 → 百分比显示）
     */
    class RtosMonitorPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit RtosMonitorPage(ICore *core, QWidget *parent = nullptr);
        ~RtosMonitorPage() override;

    signals:
        void statusMessageGenerated(const QString &text);

    private slots:
        void onFormatChanged(int index);
        void onTogglePause();
        void onClear();
        void onForceParse();
        void onConfigureStack();

    private:
        void setupToolbar(QVBoxLayout *rootLayout);
        void setupTaskTable(QVBoxLayout *rootLayout);
        void setupCpuOverview(QVBoxLayout *rootLayout);
        void subscribeDataBus();

        void handleSerialPacket(const DataPacket &packet);
        RtosSnapshot mergedSnapshot(const RtosSnapshot &incoming) const;
        void tryAutoParse();
        void updateFromSnapshot(const RtosSnapshot &snapshot);
        void updateStatusLabel(const RtosSnapshot &snapshot);
        void updateCpuOverview(const RtosSnapshot &snapshot);
        int totalStackForTask(const QString &taskName) const;
        QColor rowTintForState(RtosTaskState state) const;

        // DataBus
        DataBus *m_dataBus = nullptr;
        SubscriptionHandle m_subscription;

        // UI — toolbar
        QComboBox *m_formatCombo = nullptr;
        QToolButton *m_pauseBtn = nullptr;
        QPushButton *m_clearBtn = nullptr;
        QPushButton *m_forceParseBtn = nullptr;
        QPushButton *m_configStackBtn = nullptr;
        QLabel *m_statusLabel = nullptr;

        // UI — table
        QTableWidget *m_taskTable = nullptr;

        // UI — CPU overview
        QWidget *m_cpuOverview = nullptr;
        QVector<QWidget *> m_cpuBarWidgets;

        // 数据
        QString m_lineBuffer;           ///< 串口数据行缓存
        RtosSnapshot m_lastSnapshot;    ///< 最近一次快照
        bool m_paused = false;

        // 栈配置: taskName → totalStackWords
        QHash<QString, int> m_stackConfig;

        // 缓存中用到的列索引
        enum Column
        {
            ColNumber = 0,
            ColName,
            ColState,
            ColPriority,
            ColStackRemain,
            ColStackPercent,
            ColCpuPercent,
            ColumnCount
        };
    };

} // namespace est

#endif // EST_RTOSMONITORPAGE_H
