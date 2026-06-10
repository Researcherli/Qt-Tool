#include "services/RtosTaskParser.h"

#include <QHash>
#include <QRegularExpression>
#include <QtMath>

namespace est
{
    namespace
    {
        QString normalizedLine(QString line)
        {
            static const QRegularExpression s_ansiRe(
                QStringLiteral("\\x1B\\[[0-?]*[ -/]*[@-~]"));

            line.replace(QLatin1Char('\r'), QLatin1Char(' '));
            line.replace(QLatin1Char('\t'), QLatin1Char(' '));
            for (int i = 0; i < line.size(); ++i) {
                if (line.at(i).unicode() == 0) {
                    line[i] = QLatin1Char(' ');
                }
            }
            line.remove(s_ansiRe);
            return line.trimmed();
        }

        RtosTaskState stateFromCode(QChar stateChar)
        {
            switch (stateChar.toUpper().toLatin1())
            {
            case 'X':
                return RtosTaskState::Running;
            case 'R':
                return RtosTaskState::Ready;
            case 'B':
                return RtosTaskState::Blocked;
            case 'S':
                return RtosTaskState::Suspended;
            case 'D':
                return RtosTaskState::Deleted;
            default:
                return RtosTaskState::Unknown;
            }
        }

        bool looksLikeSeparator(const QString &line)
        {
            int markerCount = 0;
            for (const QChar ch : line)
            {
                if (ch == QLatin1Char('-') || ch == QLatin1Char('='))
                    ++markerCount;
                else if (!ch.isSpace())
                    return false;
            }
            return markerCount >= 3;
        }
    }

    // ---------------------------------------------------------------------------
    // RtosSnapshot
    // ---------------------------------------------------------------------------

    void RtosSnapshot::mergeCpuStats()
    {
        if (cpuStats.isEmpty())
            return;

        // 构建 name → cpuPercent 的快速查找表
        QHash<QString, double> cpuMap;
        for (const auto &cpu : cpuStats)
        {
            cpuMap.insert(cpu.name.trimmed(), cpu.cpuPercent);
        }

        for (auto &task : tasks)
        {
            const auto it = cpuMap.constFind(task.name.trimmed());
            if (it != cpuMap.constEnd())
                task.cpuPercent = it.value();
        }
    }

    // ---------------------------------------------------------------------------
    // RtosTaskParser
    // ---------------------------------------------------------------------------

    QVector<RtosTaskInfo> RtosTaskParser::parseTaskList(const QString &text)
    {
        QVector<RtosTaskInfo> result;

        // 从右向左解析，允许任务名里出现空格，也能跳过表头和分隔线。
        static const QRegularExpression s_re(
            QStringLiteral("^(.+?)\\s+([XRBSDrbsdx])\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s*$"));

        const QStringList lines = text.split(QLatin1Char('\n'));
        for (const QString &rawLine : lines)
        {
            // 正则前的行预处理
            QString normalized = normalizedLine(rawLine);
            // Normalize full-width spaces to ASCII spaces
            normalized.replace(QChar(0x3000), QLatin1Char(' '));
            const QString line = normalized;
            if (line.isEmpty() || looksLikeSeparator(line))
                continue;

            const QRegularExpressionMatch m = s_re.match(line);
            if (!m.hasMatch())
                continue;

            RtosTaskInfo info;
            info.name = m.captured(1).trimmed();
            if (info.name.isEmpty())
                continue;

            info.state = stateFromCode(m.captured(2).at(0));

            bool ok = false;
            info.priority = m.captured(3).toInt(&ok);
            if (!ok)
                info.priority = 0;

            info.stackRemaining = m.captured(4).toInt(&ok);
            if (!ok)
                info.stackRemaining = 0;

            info.taskNumber = m.captured(5).toInt(&ok);
            if (!ok)
                info.taskNumber = 0;

            result.append(info);
        }

        // 按任务编号排序
        std::sort(result.begin(), result.end(),
                  [](const RtosTaskInfo &a, const RtosTaskInfo &b)
                  {
                      return a.taskNumber < b.taskNumber;
                  });

        return result;
    }

    QVector<RtosTaskCpuInfo> RtosTaskParser::parseRuntimeStats(const QString &text)
    {
        QVector<RtosTaskCpuInfo> result;

        // 匹配格式: name(空白)绝对时间(空白)百分比，可接受 <1%、1%、1.5% 和 1,5%。
        static const QRegularExpression s_re(
            QStringLiteral("^(.+?)\\s+(\\d+)\\s+((?:<\\s*)?\\d+(?:[\\.,]\\d+)?)\\s*%?\\s*$"));
        static const QRegularExpression s_taskListRe(
            QStringLiteral("^(.+?)\\s+([XRBSDrbsdx])\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s*$"));

        double totalPercent = 0.0;

        const QStringList lines = text.split(QLatin1Char('\n'));
        for (const QString &rawLine : lines)
        {
            const QString line = normalizedLine(rawLine);
            if (line.isEmpty() || looksLikeSeparator(line))
                continue;
            if (s_taskListRe.match(line).hasMatch())
                continue;

            const QRegularExpressionMatch m = s_re.match(line);
            if (!m.hasMatch())
                continue;

            RtosTaskCpuInfo info;
            info.name = m.captured(1).trimmed();
            if (info.name.isEmpty())
                continue;

            bool ok = false;
            info.absoluteTime = m.captured(2).toLongLong(&ok);
            if (!ok)
                info.absoluteTime = 0;

            QString percentText = m.captured(3).remove(QLatin1Char(' '));
            const bool lessThanOne = percentText.startsWith(QLatin1Char('<'));
            percentText.remove(QLatin1Char('<'));
            percentText.replace(QLatin1Char(','), QLatin1Char('.'));
            info.cpuPercent = percentText.toDouble(&ok);
            if (!ok)
                info.cpuPercent = 0.0;
            if (lessThanOne && info.cpuPercent <= 1.0)
                info.cpuPercent = 0.5;

            totalPercent += info.cpuPercent;
            result.append(info);
        }

        // CPU% 归一化：如果总和明显偏离 100%，按比例归一化
        if (!result.isEmpty() && (totalPercent < 90.0 || totalPercent > 101.0))
        {
            const double ratio = 100.0 / totalPercent;
            for (auto &info : result)
                info.cpuPercent *= ratio;
        }

        // 按 CPU% 降序排列
        std::sort(result.begin(), result.end(),
                  [](const RtosTaskCpuInfo &a, const RtosTaskCpuInfo &b)
                  {
                      return a.cpuPercent > b.cpuPercent;
                  });

        return result;
    }

    RtosSnapshot RtosTaskParser::parseSnapshot(const QString &text,
                                                qint64 timestamp)
    {
        RtosSnapshot snap;
        snap.timestamp = timestamp;

        const QString cleaned = text.trimmed();
        if (cleaned.isEmpty())
            return snap;

        // 先尝试解析 vTaskList 格式
        QVector<RtosTaskInfo> tasks = parseTaskList(cleaned);
        if (!tasks.isEmpty())
        {
            snap.tasks = tasks;
            // 如果同一段文本中也包含 runtime stats，尝试解析
            snap.cpuStats = parseRuntimeStats(cleaned);
            snap.mergeCpuStats();
            return snap;
        }

        // 尝试解析 runtime stats 格式
        QVector<RtosTaskCpuInfo> cpuStats = parseRuntimeStats(cleaned);
        if (!cpuStats.isEmpty())
        {
            snap.cpuStats = cpuStats;
            return snap;
        }

        return snap;
    }

    // ---- 工具方法 ----

    QString RtosTaskParser::stateToString(RtosTaskState state)
    {
        switch (state)
        {
        case RtosTaskState::Running:
            return QStringLiteral("运行");
        case RtosTaskState::Ready:
            return QStringLiteral("就绪");
        case RtosTaskState::Blocked:
            return QStringLiteral("阻塞");
        case RtosTaskState::Suspended:
            return QStringLiteral("挂起");
        case RtosTaskState::Deleted:
            return QStringLiteral("删除");
        default:
            return QStringLiteral("未知");
        }
    }

    QColor RtosTaskParser::stateToColor(RtosTaskState state)
    {
        switch (state)
        {
        case RtosTaskState::Running:
            return QColor(QStringLiteral("#00A6A6")); // 青
        case RtosTaskState::Ready:
            return QColor(QStringLiteral("#4CAF50")); // 绿色
        case RtosTaskState::Blocked:
            return QColor(QStringLiteral("#2196F3")); // 蓝色
        case RtosTaskState::Suspended:
            return QColor(QStringLiteral("#9E9E9E")); // 灰色
        case RtosTaskState::Deleted:
            return QColor(QStringLiteral("#F44336")); // 红色
        default:
            return QColor(QStringLiteral("#BDBDBD")); // 浅灰
        }
    }

    QString RtosTaskParser::stateToSymbol(RtosTaskState state)
    {
        switch (state)
        {
        case RtosTaskState::Running:
            return QStringLiteral("运行");
        case RtosTaskState::Ready:
            return QStringLiteral("就绪");
        case RtosTaskState::Blocked:
            return QStringLiteral("阻塞");
        case RtosTaskState::Suspended:
            return QStringLiteral("挂起");
        case RtosTaskState::Deleted:
            return QStringLiteral("删除");
        default:
            return QStringLiteral("未知");
        }
    }

} // namespace est
