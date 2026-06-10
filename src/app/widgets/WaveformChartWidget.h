#ifndef EST_WAVEFORMCHARTWIDGET_H
#define EST_WAVEFORMCHARTWIDGET_H

#include <QChartView>
#include <QColor>
#include <QHash>
#include <QRegularExpression>
#include <QString>
#include <QVector>

class QChart;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QLineSeries;
class QPushButton;
class QToolButton;
class QValueAxis;
class QVBoxLayout;

namespace est
{

    struct SeriesConfig
    {
        QString name;
        QColor color;
        QString parserPattern;      ///< 正则表达式，捕获组提取数值
        QRegularExpression parserExpression;
        QLineSeries *series = nullptr;
        QVector<QPointF> buffer;    ///< 滚动缓冲区
        int maxPoints = 5000;       ///< 最大点数
        double lastTimestamp = 0;
        double lastValue = 0;
    };

    /**
     * 实时波形图表控件 — 基于 Qt Charts 的滚动曲线图。
     *
     * 支持多序列叠加显示、正则解析数据提取、暂停/继续、清空、时间窗口调整。
     */
    class WaveformChartWidget : public QChartView
    {
        Q_OBJECT

    public:
        explicit WaveformChartWidget(QWidget *parent = nullptr);
        ~WaveformChartWidget() override;

        /// 添加一个数据序列
        int addSeries(const QString &name, const QColor &color,
                      const QString &parserPattern,
                      QString *errorMessage = nullptr);

        /// 移除指定索引的序列
        void removeSeries(int index);

        /// 馈入数据 — 尝试匹配所有序列的解析器
        void feedData(const QString &channel, qint64 timestampMs,
                      const QByteArray &rawPayload);

        /// 暂停/继续数据更新
        void setPaused(bool paused);
        bool isPaused() const { return m_paused; }

        /// 清空所有序列数据
        void clearAll();

        /// 设置滚动时间窗口（秒）
        void setTimeWindow(double seconds);
        double timeWindow() const { return m_timeWindowSec; }

        /// 序列数量
        int seriesCount() const { return m_series.size(); }
        int totalPointCount() const;

        /// 获取序列配置（只读）
        const SeriesConfig &seriesConfig(int index) const { return m_series.at(index); }

    signals:
        void seriesAdded(int index);
        void seriesRemoved(int index);
        void allCleared();
        void dataAccepted(int seriesIndex, double value, double timestampSec);

    private:
        double tryParse(const SeriesConfig &cfg, const QByteArray &payload) const;
        QVector<double> parseValues(const SeriesConfig &cfg, const QByteArray &payload) const;
        void updateAxes();
        void pruneBuffer(SeriesConfig &cfg);
        void appendPoint(SeriesConfig &cfg, int seriesIndex, double timestampSec, double value);

        QChart *m_chart = nullptr;
        QValueAxis *m_axisX = nullptr;
        QValueAxis *m_axisY = nullptr;

        QVector<SeriesConfig> m_series;
        bool m_paused = false;
        double m_timeWindowSec = 10.0;      ///< 滚动时间窗口（秒）
        qint64 m_startTimestamp = -1;       ///< 首个数据点的时间戳（用于归一化）
        double m_currentMaxX = 10.0;

        QColor m_nextColor;                 ///< 自动分配下一个颜色
        int m_colorIndex = 0;

        static const QVector<QColor> kDefaultColors;
    };

} // namespace est

#endif // EST_WAVEFORMCHARTWIDGET_H
