#ifndef EST_HOMEPAGE_H
#define EST_HOMEPAGE_H

#include <QWidget>

namespace est
{

    class ICore;

    class HomePage : public QWidget
    {
        Q_OBJECT

    public:
        explicit HomePage(ICore *core, QWidget *parent = nullptr);

        void setSerialStatus(const QString &text, bool connected);
        void setCurrentFileStatus(const QString &text);
        void setTransferStats(qint64 txBytes, qint64 rxBytes);
        void appendSystemLog(const QString &text);
        void refreshRecentRecords();

    signals:
        void openSerialAssistantRequested();
        void openDataConvertRequested();
        void openFileCompareRequested();
        void openBinAnalyzerRequested();
        void openWaveformRequested();
        void openRtosMonitorRequested();
        void openProtocolDecoderRequested();
        void openSerialConsoleRequested();
        void openCANBusRequested();
        void openUserGuideRequested();
    };

} // namespace est

#endif // EST_HOMEPAGE_H
