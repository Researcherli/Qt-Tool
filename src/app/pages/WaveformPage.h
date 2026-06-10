#ifndef EST_WAVEFORMPAGE_H
#define EST_WAVEFORMPAGE_H

#include <QWidget>

#include "databus/SubscriptionHandle.h"

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QToolButton;

namespace est
{

    class DataBus;
    class ICore;
    struct DataPacket;
    struct SubscriptionHandle;
    class WaveformChartWidget;

    /**
     * 实时波形页面 — 订阅串口数据流，实时绘制滚动曲线图。
     *
     * 用法：
     *   1. 点击"添加序列"选择解析规则和通道
     *   2. 打开串口工具开始接收数据
     *   3. 波形页面自动解析并绘制匹配的数据
     */
    class WaveformPage : public QWidget
    {
        Q_OBJECT

    public:
        explicit WaveformPage(ICore *core, QWidget *parent = nullptr);
        ~WaveformPage() override;

    signals:
        void statusMessageGenerated(const QString &text);

    private slots:
        void onAddSeries();
        void onRemoveSeries();
        void onTogglePause();
        void onClear();
        void onTimeWindowChanged(double seconds);

    private:
        void setupToolbar();
        void subscribeDataBus();
        void updateStatusText();

        DataBus *m_dataBus = nullptr;
        WaveformChartWidget *m_chartWidget = nullptr;

        // Toolbar widgets
        QPushButton *m_addSeriesBtn = nullptr;
        QPushButton *m_removeSeriesBtn = nullptr;
        QToolButton *m_pauseBtn = nullptr;
        QPushButton *m_clearBtn = nullptr;
        QDoubleSpinBox *m_timeWindowSpin = nullptr;
        QLabel *m_statusLabel = nullptr;

        // DataBus 订阅
        SubscriptionHandle m_subscription;

        // 当前选中序列索引
        int m_selectedSeriesIndex = -1;
        int m_receivedPackets = 0;
    };

} // namespace est

#endif // EST_WAVEFORMPAGE_H
