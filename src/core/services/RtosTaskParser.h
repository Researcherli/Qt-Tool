#ifndef EST_RTOSTASKPARSER_H
#define EST_RTOSTASKPARSER_H

#include <QColor>
#include <QString>
#include <QVector>

namespace est
{

    // ---------------------------------------------------------------------------
    // 数据结构
    // ---------------------------------------------------------------------------

    /// FreeRTOS vTaskList() 中的任务状态码
    enum class RtosTaskState
    {
        Running,    // 'X'
        Ready,      // 'R'
        Blocked,    // 'B'
        Suspended,  // 'S'
        Deleted,    // 'D'
        Unknown
    };

    /// vTaskList() 解析结果 — 单个任务
    struct RtosTaskInfo
    {
        QString name;
        RtosTaskState state = RtosTaskState::Unknown;
        int priority = 0;
        int stackRemaining = 0; ///< 单位: words（FreeRTOS 原始单位）
        int taskNumber = 0;
        double cpuPercent = -1.0;
    };

    /// vTaskGetRunTimeStats() 解析结果 — 单个任务的 CPU 数据
    struct RtosTaskCpuInfo
    {
        QString name;
        qint64 absoluteTime = 0;
        double cpuPercent = 0.0;
    };

    /// 一次快照解析结果（合并两种数据）
    struct RtosSnapshot
    {
        qint64 timestamp = 0;
        QVector<RtosTaskInfo> tasks;
        QVector<RtosTaskCpuInfo> cpuStats;

        bool isEmpty() const { return tasks.isEmpty() && cpuStats.isEmpty(); }
        int taskCount() const { return tasks.size(); }

        /// 将 cpuStats 按 name 合并到 tasks（找不到匹配的丢弃）
        void mergeCpuStats();
    };

    // ---------------------------------------------------------------------------
    // 解析器
    // ---------------------------------------------------------------------------

    /**
     * FreeRTOS 任务输出解析器 — 解析 vTaskList() 和 vTaskGetRunTimeStats() 输出。
     *
     * 纯静态工具类，无状态，线程安全。
     * 支持格式自动检测、容错跳过无效行。
     */
    class RtosTaskParser
    {
    public:
        /// 解析 vTaskList() 输出 → 返回任务列表
        static QVector<RtosTaskInfo> parseTaskList(const QString &text);

        /// 解析 vTaskGetRunTimeStats() 输出 → 返回 CPU 数据
        static QVector<RtosTaskCpuInfo> parseRuntimeStats(const QString &text);

        /// 自动检测格式并解析整段文本
        static RtosSnapshot parseSnapshot(const QString &text,
                                           qint64 timestamp = 0);

        // ---- 工具方法 ----

        /// 状态码 → 中文描述
        static QString stateToString(RtosTaskState state);

        /// 状态码 → 颜色
        static QColor stateToColor(RtosTaskState state);

        /// 状态码 → 状态符号（中文单字）
        static QString stateToSymbol(RtosTaskState state);
    };

} // namespace est

#endif // EST_RTOSTASKPARSER_H
