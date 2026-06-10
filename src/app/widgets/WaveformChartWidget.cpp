#include "widgets/WaveformChartWidget.h"

#include <QChart>
#include <QLineSeries>
#include <QPainter>
#include <QValueAxis>
#include <QtMath>
#include <limits>

namespace est
{

    const QVector<QColor> WaveformChartWidget::kDefaultColors = {
        QColor(QStringLiteral("#4CAF50")),   // 绿色
        QColor(QStringLiteral("#2196F3")),   // 蓝色
        QColor(QStringLiteral("#FF9800")),   // 橙色
        QColor(QStringLiteral("#E91E63")),   // 粉色
        QColor(QStringLiteral("#9C27B0")),   // 紫色
        QColor(QStringLiteral("#00BCD4")),   // 青色
        QColor(QStringLiteral("#FFEB3B")),   // 黄色
        QColor(QStringLiteral("#F44336")),   // 红色
    };

    WaveformChartWidget::WaveformChartWidget(QWidget *parent)
        : QChartView(parent)
    {
        setObjectName(QStringLiteral("waveformChart"));
        setRenderHint(QPainter::Antialiasing);
        setRubberBand(QChartView::RectangleRubberBand);

        m_chart = new QChart();
        m_chart->setTitle(tr("实时波形"));
        m_chart->setAnimationOptions(QChart::NoAnimation);
        m_chart->legend()->setAlignment(Qt::AlignBottom);
        m_chart->legend()->setShowToolTips(true);
        m_chart->setBackgroundRoundness(4);

        m_axisX = new QValueAxis();
        m_axisX->setTitleText(tr("时间 (秒)"));
        m_axisX->setLabelFormat(QStringLiteral("%.1f"));
        m_axisX->setRange(0, m_timeWindowSec);
        m_axisX->setGridLineVisible(true);
        m_axisX->setTickCount(6);

        m_axisY = new QValueAxis();
        m_axisY->setTitleText(tr("数值"));
        m_axisY->setLabelFormat(QStringLiteral("%.1f"));
        m_axisY->setRange(-10, 10);
        m_axisY->setGridLineVisible(true);
        m_axisY->setTickCount(5);

        m_chart->addAxis(m_axisX, Qt::AlignBottom);
        m_chart->addAxis(m_axisY, Qt::AlignLeft);

        setChart(m_chart);
    }

    WaveformChartWidget::~WaveformChartWidget() = default;

    int WaveformChartWidget::addSeries(const QString &name, const QColor &color,
                                        const QString &parserPattern,
                                        QString *errorMessage)
    {
        if (parserPattern.trimmed().isEmpty())
        {
            if (errorMessage != nullptr)
                *errorMessage = tr("解析规则不能为空。");
            return -1;
        }

        SeriesConfig cfg;
        cfg.name = name;
        cfg.color = color.isValid() ? color : kDefaultColors[m_colorIndex % kDefaultColors.size()];
        cfg.parserPattern = parserPattern.trimmed();

        const bool isPreset = cfg.parserPattern.startsWith(QStringLiteral("__"))
                              && cfg.parserPattern.endsWith(QStringLiteral("__"));
        if (!isPreset)
        {
            cfg.parserExpression = QRegularExpression(cfg.parserPattern);
            if (!cfg.parserExpression.isValid())
            {
                if (errorMessage != nullptr)
                    *errorMessage = tr("正则表达式无效：%1").arg(cfg.parserExpression.errorString());
                return -1;
            }
            if (cfg.parserExpression.captureCount() < 1)
            {
                if (errorMessage != nullptr)
                    *errorMessage = tr("自定义正则至少需要一个捕获组，用来提取数值。");
                return -1;
            }
        }

        cfg.series = new QLineSeries();
        cfg.series->setName(cfg.name);
        cfg.series->setColor(cfg.color);
        cfg.series->setPen(QPen(cfg.color, 2));

        m_chart->addSeries(cfg.series);
        cfg.series->attachAxis(m_axisX);
        cfg.series->attachAxis(m_axisY);

        m_series.append(cfg);
        m_colorIndex++;

        emit seriesAdded(m_series.size() - 1);
        return m_series.size() - 1;
    }

    void WaveformChartWidget::removeSeries(int index)
    {
        if (index < 0 || index >= m_series.size())
            return;

        SeriesConfig &cfg = m_series[index];
        m_chart->removeSeries(cfg.series);
        delete cfg.series;
        m_series.removeAt(index);
        updateAxes();

        emit seriesRemoved(index);
    }

    void WaveformChartWidget::feedData(const QString &channel, qint64 timestampMs,
                                        const QByteArray &rawPayload)
    {
        if (m_paused || rawPayload.isEmpty())
            return;

        // 记录首个时间戳（绝对时间 → 相对秒数）
        if (m_startTimestamp < 0)
            m_startTimestamp = timestampMs;

        const double elapsedSec = (timestampMs - m_startTimestamp) / 1000.0;

        for (int i = 0; i < m_series.size(); ++i)
        {
            SeriesConfig &cfg = m_series[i];
            const QVector<double> values = parseValues(cfg, rawPayload);
            if (values.isEmpty())
                continue;

            const double sampleStep = values.size() > 1 ? 0.001 : 0.0;
            for (int valueIndex = 0; valueIndex < values.size(); ++valueIndex)
            {
                appendPoint(cfg, i, elapsedSec + sampleStep * valueIndex, values.at(valueIndex));
            }

            cfg.series->replace(cfg.buffer);
        }

        // 自动调整 X 轴范围
        updateAxes();
    }

    double WaveformChartWidget::tryParse(const SeriesConfig &cfg,
                                          const QByteArray &payload) const
    {
        if (cfg.parserPattern.isEmpty())
            return qQNaN();

        const QString text = QString::fromUtf8(payload).trimmed();
        if (text.isEmpty())
            return qQNaN();

        // 特殊预置解析器
        if (cfg.parserPattern == QStringLiteral("__hex_byte_0__"))
        {
            return static_cast<double>(static_cast<quint8>(payload.at(0)));
        }
        if (cfg.parserPattern == QStringLiteral("__hex_byte_1__"))
        {
            return payload.size() > 1
                       ? static_cast<double>(static_cast<quint8>(payload.at(1)))
                       : qQNaN();
        }
        if (cfg.parserPattern == QStringLiteral("__hex_word_le__"))
        {
            return payload.size() >= 2
                       ? static_cast<double>(static_cast<quint16>(
                             static_cast<quint8>(payload.at(0)) |
                             (static_cast<quint8>(payload.at(1)) << 8)))
                       : qQNaN();
        }
        if (cfg.parserPattern == QStringLiteral("__first_float__"))
        {
            static const QRegularExpression re(QStringLiteral("(-?\\d+\\.?\\d*(?:[eE][+-]?\\d+)?)"));
            QRegularExpressionMatch m = re.match(text);
            if (m.hasMatch())
            {
                bool ok = false;
                double val = m.captured(1).toDouble(&ok);
                return ok ? val : qQNaN();
            }
            return qQNaN();
        }
        if (cfg.parserPattern == QStringLiteral("__first_int__"))
        {
            static const QRegularExpression re(QStringLiteral("(-?\\d+)"));
            QRegularExpressionMatch m = re.match(text);
            if (m.hasMatch())
            {
                bool ok = false;
                double val = m.captured(1).toDouble(&ok);
                return ok ? val : qQNaN();
            }
            return qQNaN();
        }

        QRegularExpressionMatch m = cfg.parserExpression.match(text);
        if (m.hasMatch() && m.lastCapturedIndex() >= 1)
        {
            bool ok = false;
            double val = m.captured(1).toDouble(&ok);
            return ok ? val : qQNaN();
        }

        return qQNaN();
    }

    QVector<double> WaveformChartWidget::parseValues(const SeriesConfig &cfg,
                                                      const QByteArray &payload) const
    {
        QVector<double> values;
        if (cfg.parserPattern.startsWith(QStringLiteral("__")))
        {
            const double value = tryParse(cfg, payload);
            if (!qIsNaN(value))
                values.append(value);
            return values;
        }

        const QString text = QString::fromUtf8(payload);
        QRegularExpressionMatchIterator it = cfg.parserExpression.globalMatch(text);
        while (it.hasNext())
        {
            const QRegularExpressionMatch match = it.next();
            if (match.lastCapturedIndex() < 1)
                continue;

            bool ok = false;
            const double value = match.captured(1).toDouble(&ok);
            if (ok)
                values.append(value);
        }

        return values;
    }

    void WaveformChartWidget::appendPoint(SeriesConfig &cfg, int seriesIndex,
                                           double timestampSec, double value)
    {
        cfg.buffer.append(QPointF(timestampSec, value));
        cfg.lastTimestamp = timestampSec;
        cfg.lastValue = value;
        pruneBuffer(cfg);
        emit dataAccepted(seriesIndex, value, timestampSec);
    }

    void WaveformChartWidget::updateAxes()
    {
        // X 轴：滚动时间窗口
        double maxX = 0;
        double minX = 0;
        bool hasData = false;
        for (const auto &cfg : m_series)
        {
            if (cfg.buffer.isEmpty())
                continue;
            if (!hasData)
            {
                minX = cfg.buffer.first().x();
                maxX = cfg.buffer.last().x();
                hasData = true;
            }
            else
            {
                minX = qMin(minX, cfg.buffer.first().x());
                maxX = qMax(maxX, cfg.buffer.last().x());
            }
        }

        if (!hasData)
        {
            m_axisX->setRange(0, m_timeWindowSec);
            return;
        }

        const double windowStart = qMax(0.0, maxX - m_timeWindowSec);
        m_axisX->setRange(windowStart, windowStart + m_timeWindowSec);

        // Y 轴：自动范围（留 10% 边距）
        double yMin = std::numeric_limits<double>::max();
        double yMax = std::numeric_limits<double>::lowest();
        for (const auto &cfg : m_series)
        {
            // 仅统计窗口内的点
            for (const auto &pt : cfg.buffer)
            {
                if (pt.x() >= windowStart)
                {
                    yMin = qMin(yMin, pt.y());
                    yMax = qMax(yMax, pt.y());
                }
            }
        }

        if (yMin <= yMax)
        {
            const double span = qMax(yMax - yMin, 1.0);
            const double padding = span * 0.1;
            double newMin = yMin - padding;
            double newMax = yMax + padding;
            // 避免零范围
            if (qFuzzyCompare(newMin, newMax))
            {
                newMin -= 1.0;
                newMax += 1.0;
            }
            m_axisY->setRange(newMin, newMax);
        }
    }

    void WaveformChartWidget::pruneBuffer(SeriesConfig &cfg)
    {
        if (cfg.buffer.isEmpty())
            return;

        const double cutoff = cfg.lastTimestamp - m_timeWindowSec;
        // 批量移除窗口外的点
        int removeCount = 0;
        for (const auto &pt : cfg.buffer)
        {
            if (pt.x() < cutoff)
                ++removeCount;
            else
                break;
        }

        if (removeCount > 0)
            cfg.buffer.remove(0, removeCount);

        // 限制最大点数
        if (cfg.buffer.size() > cfg.maxPoints)
            cfg.buffer.remove(0, cfg.buffer.size() - cfg.maxPoints);
    }

    void WaveformChartWidget::setPaused(bool paused)
    {
        m_paused = paused;
    }

    void WaveformChartWidget::clearAll()
    {
        for (auto &cfg : m_series)
        {
            cfg.buffer.clear();
            cfg.series->clear();
            cfg.lastTimestamp = 0;
        }
        m_startTimestamp = -1;
        m_currentMaxX = m_timeWindowSec;
        m_axisX->setRange(0, m_timeWindowSec);
        m_axisY->setRange(-10, 10);

        emit allCleared();
    }

    void WaveformChartWidget::setTimeWindow(double seconds)
    {
        if (seconds < 1.0)
            seconds = 1.0;
        if (seconds > 300.0)
            seconds = 300.0;

        m_timeWindowSec = seconds;

        // 裁剪所有序列的缓冲区
        for (auto &cfg : m_series)
            pruneBuffer(cfg);

        updateAxes();
    }

    int WaveformChartWidget::totalPointCount() const
    {
        int total = 0;
        for (const auto &cfg : m_series)
            total += cfg.buffer.size();
        return total;
    }

} // namespace est
