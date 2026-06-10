#ifndef EST_APPSHELL_H
#define EST_APPSHELL_H

#include "plugin/ICore.h"

#include <QMainWindow>
#include <QHash>

class QEvent;
class QLabel;
class QDockWidget;
class QStackedWidget;

namespace est
{

    class BinAnalyzerPage;
    class CANBusPage;
    class DataConvertPage;
    class DataBus;
    class DataLogger;
    class DataPlayer;
    class FileComparePage;
    class HomePage;
    class PluginRegistry;
    class PluginLoader;
    class PlaybackControl;
    class ProtocolDecoderPage;
    class RecentRecordManager;
    class SerialConsolePage;
    class RtosMonitorPage;
    class SerialAssistantPage;
    class SideNavBar;
    class TransportRegistry;
    class UserGuidePage;
    class WaveformPage;

    /**
     * 应用壳 — 主窗口 + ICore 实现 + 插件管理。
     *
     * 职责：
     *   1. 实现 ICore 接口，为插件提供核心服务
     *   2. 管理 MainWindow + DockWidget 布局
     *   3. 协调插件加载和视图注册
     */
    class AppShell : public QMainWindow, public ICore
    {
        Q_OBJECT

    public:
        explicit AppShell(QWidget *parent = nullptr);
        ~AppShell() override;

        /// 初始化核心服务和插件
        bool initialize();

        /// ICore 接口实现
        DataBus *dataBus() const override;
        PluginRegistry *pluginRegistry() const override;
        TransportRegistry *transportRegistry() const override;
        RecentRecordManager *recentRecordManager() const override;

    protected:
        void changeEvent(QEvent *event) override;

    private:
        void setupStatusBar();
        void setupWorkspace();
        void registerPluginViews();
        void initSideNavBar();
        void initPages();
        void initSignalConnections();
        void initRecordingPlayback();
        void updateSideNavExpansionMode();
        void switchToPage(const QString &pageId);
        void addSystemLog(const QString &text);
        void updateSerialStatus(const QString &text, bool connected);
        void updateCurrentPageTitle(const QString &title);
        void updateCurrentFileStatus(const QString &text);
        void updateTransferStats(qint64 txBytes, qint64 rxBytes);
        void toggleRecording();
        void updateRecordingActionUi(bool recording);
        QString makeDraftRecordingPath() const;
        void saveRecordingAs();
        void loadPlaybackFile();
        void closePlayback();
        void togglePlaybackPanel();

        DataBus *m_dataBus = nullptr;
        PluginRegistry *m_pluginRegistry = nullptr;
        PluginLoader *m_pluginLoader = nullptr;
        TransportRegistry *m_transportRegistry = nullptr;
        RecentRecordManager *m_recentRecordManager = nullptr;
        QStackedWidget *m_pageStack = nullptr;
        SideNavBar *m_sideNavBar = nullptr;
        HomePage *m_homePage = nullptr;
        SerialAssistantPage *m_serialAssistantPage = nullptr;
        DataConvertPage *m_dataConvertPage = nullptr;
        FileComparePage *m_fileComparePage = nullptr;
        BinAnalyzerPage *m_binAnalyzerPage = nullptr;
        WaveformPage *m_waveformPage = nullptr;
        RtosMonitorPage *m_rtosMonitorPage = nullptr;
        UserGuidePage *m_userGuidePage = nullptr;
        ProtocolDecoderPage *m_protocolDecoderPage = nullptr;
        SerialConsolePage *m_serialConsolePage = nullptr;
        CANBusPage *m_canBusPage = nullptr;
        DataLogger *m_dataLogger = nullptr;
        DataPlayer *m_dataPlayer = nullptr;
        PlaybackControl *m_playbackControl = nullptr;
        QDockWidget *m_playbackDock = nullptr;
        QLabel *m_recordStatus = nullptr;
        QLabel *m_readyLabel = nullptr;
        QLabel *m_serialStateLabel = nullptr;
        QLabel *m_fileStateLabel = nullptr;
        QLabel *m_txStateLabel = nullptr;
        QLabel *m_rxStateLabel = nullptr;
        QLabel *m_versionLabel = nullptr;
        QLabel *m_titleLabel = nullptr;
        QLabel *m_pageTitleLabel = nullptr;
        QString m_currentPageId;
        QString m_pendingRecordingPath;
        int m_pendingRecordingFrames = 0;
        qint64 m_pendingRecordingBytes = 0;
        QHash<QString, int> m_pageIndexes;
    };

} // namespace est

#endif // EST_APPSHELL_H
