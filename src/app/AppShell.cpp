#include "AppShell.h"

#include "databus/DataBus.h"
#include "pages/BinAnalyzerPage.h"
#include "pages/DataConvertPage.h"
#include "pages/FileComparePage.h"
#include "pages/HomePage.h"
#include "pages/SerialAssistantPage.h"
#include "pages/RtosMonitorPage.h"
#include "pages/CANBusPage.h"
#include "pages/ProtocolDecoderPage.h"
#include "pages/SerialConsolePage.h"
#include "pages/UserGuidePage.h"
#include "pages/WaveformPage.h"
#include "plugin/IViewFactory.h"
#include "plugin/PluginLoader.h"
#include "plugin/PluginRegistry.h"
#include "services/RecentRecordManager.h"
#include "services/AppPaths.h"
#include "services/DataLogger.h"
#include "transport/SerialTransport.h"
#include "transport/TransportRegistry.h"
#include "widgets/ModuleIconFactory.h"
#include "widgets/PlaybackControl.h"
#include "widgets/SideNavBar.h"

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDockWidget>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QStyleFactory>
#include <QTime>
#include <QToolBar>

namespace est
{

    namespace
    {

        const QString kHomePageId = QStringLiteral("home");
        const QString kSerialPageId = QStringLiteral("serial");
        const QString kDataConvertPageId = QStringLiteral("data_convert");
        const QString kFileComparePageId = QStringLiteral("file_compare");
        const QString kBinAnalyzerPageId = QStringLiteral("bin_analyzer");
        const QString kWaveformPageId = QStringLiteral("waveform");
        const QString kRtosMonitorPageId = QStringLiteral("rtos_monitor");
        const QString kUserGuidePageId = QStringLiteral("user_guide");
        const QString kProtocolDecoderPageId = QStringLiteral("protocol_decoder");
        const QString kSerialConsolePageId = QStringLiteral("serial_console");
        const QString kCANBusPageId = QStringLiteral("can_bus");
        const QString kDataLogPageId = QStringLiteral("data_log");
        const QString kRecordActionId = QStringLiteral("record_action");
        const QString kSaveRecordingActionId = QStringLiteral("save_recording_action");

        QString makeTimestampedLog(const QString &text)
        {
            return QStringLiteral("[%1] %2")
                .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), text);
        }

        QIcon moduleIcon(const QString &type)
        {
            return QIcon(makeModuleIconPixmap(type, 48));
        }

    } // namespace

    AppShell::AppShell(QWidget *parent)
        : QMainWindow(parent)
        , m_dataBus(new DataBus(this))
        , m_pluginRegistry(new PluginRegistry(this))
        , m_transportRegistry(new TransportRegistry(this))
        , m_recentRecordManager(new RecentRecordManager(this))
    {
        setWindowTitle(tr("EST Studio"));
        resize(1360, 860);
        setMinimumSize(1180, 720);

        m_transportRegistry->registerFactory(QStringLiteral("serial"), []()
                                             { return new SerialTransport(); });

        // 录制 & 回放引擎
        m_dataLogger = new DataLogger(this);
        m_dataPlayer = new DataPlayer(this);

        setupWorkspace();
        // 状态栏已迁移到各页面内部，主窗口始终隐藏状态栏
        statusBar()->setVisible(false);
    }

    AppShell::~AppShell() = default;

    bool AppShell::initialize()
    {
        if (m_pluginLoader == nullptr)
        {
            m_pluginLoader = new PluginLoader(this, m_pluginRegistry, this);
            // 插件目录：可执行文件同级的 plugins/ 子目录
            QString pluginPath = QApplication::applicationDirPath() + "/plugins";
            m_pluginLoader->addPluginPath(pluginPath);

            // 开发模式：构建目录
#ifdef EST_PLUGIN_DIR
            m_pluginLoader->addPluginPath(EST_PLUGIN_DIR);
#endif

            connect(m_pluginLoader, &PluginLoader::pluginLoaded,
                    this, [this](const QString &name)
                    { addSystemLog(tr("后台已加载扩展：%1").arg(name)); });
        }

        if (!m_pluginLoader->loadAll())
        {
            addSystemLog(tr("部分扩展加载失败，请检查日志"));
        }

        registerPluginViews();

        updateSerialStatus(tr("未连接"), false);
        updateCurrentFileStatus(tr("文件：无"));
        updateTransferStats(0, 0);
        if (m_homePage != nullptr)
        {
            m_homePage->refreshRecentRecords();
        }
        addSystemLog(tr("软件启动完成"));
        addSystemLog(tr("当前未连接任何设备"));
        statusBar()->showMessage(tr("系统初始化完成"), 3000);
        return true;
    }

    DataBus *AppShell::dataBus() const { return m_dataBus; }
    PluginRegistry *AppShell::pluginRegistry() const { return m_pluginRegistry; }
    TransportRegistry *AppShell::transportRegistry() const { return m_transportRegistry; }
    RecentRecordManager *AppShell::recentRecordManager() const { return m_recentRecordManager; }

    void AppShell::changeEvent(QEvent *event)
    {
        QMainWindow::changeEvent(event);

        if (event->type() == QEvent::WindowStateChange)
        {
            updateSideNavExpansionMode();
        }
    }

    void AppShell::setupWorkspace()
    {
        QWidget *central = new QWidget(this);
        central->setObjectName(QStringLiteral("workspaceRoot"));

        auto *layout = new QHBoxLayout(central);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        initSideNavBar();
        initPages();

        layout->addWidget(m_sideNavBar);
        layout->addWidget(m_pageStack, 1);

        setCentralWidget(central);
        updateSideNavExpansionMode();

        initSignalConnections();
        initRecordingPlayback();

        switchToPage(kHomePageId);
    }

    void AppShell::initSideNavBar()
    {
        m_sideNavBar = new SideNavBar(this);
        m_sideNavBar->addNavigationItem(kHomePageId, tr("首页"),
                                         QIcon(makeHomeIconPixmap(48)));
        // 通信层
        m_sideNavBar->addNavigationItem(kSerialPageId, tr("串口工具"),
                                         moduleIcon(QStringLiteral("serial")));
        m_sideNavBar->addNavigationItem(kSerialConsolePageId, tr("终端控制台"),
                                         moduleIcon(QStringLiteral("terminal")));
        // 数据处理
        m_sideNavBar->addNavigationItem(kDataConvertPageId, tr("数据转换"),
                                         moduleIcon(QStringLiteral("convert")));
        m_sideNavBar->addNavigationItem(kProtocolDecoderPageId, tr("协议解析"),
                                         moduleIcon(QStringLiteral("protocol")));
        // 可视化
        m_sideNavBar->addNavigationItem(kWaveformPageId, tr("波形图"),
                                         moduleIcon(QStringLiteral("waveform")));
        m_sideNavBar->addNavigationItem(kRtosMonitorPageId, tr("RTOS 监控"),
                                         moduleIcon(QStringLiteral("rtos")));
        // 文件工具
        m_sideNavBar->addNavigationItem(kBinAnalyzerPageId, tr("BIN 文件分析"),
                                         moduleIcon(QStringLiteral("bin")));
        m_sideNavBar->addNavigationItem(kFileComparePageId, tr("文件比较"),
                                         moduleIcon(QStringLiteral("compare")));
        // CAN
        m_sideNavBar->addNavigationItem(kCANBusPageId, tr("CAN 总线"),
                                         moduleIcon(QStringLiteral("can")));
        // 文档
        m_sideNavBar->addNavigationItem(kUserGuidePageId, tr("使用手册"),
                                         moduleIcon(QStringLiteral("guide")));
        m_sideNavBar->addBottomNavigationItem(kRecordActionId, tr("开始录制"),
                                              moduleIcon(QStringLiteral("record_start")));
        m_sideNavBar->addBottomNavigationItem(kSaveRecordingActionId, tr("保存录制"),
                                              moduleIcon(QStringLiteral("save")));
        m_sideNavBar->addBottomNavigationItem(kDataLogPageId, tr("日志回放"),
                                              moduleIcon(QStringLiteral("playback")));
    }

    void AppShell::initPages()
    {
        m_pageStack = new QStackedWidget(this);
        m_pageStack->setObjectName(QStringLiteral("pageStack"));

        m_homePage = new HomePage(this, this);
        m_serialAssistantPage = new SerialAssistantPage(this, this);
        m_dataConvertPage = new DataConvertPage(this);
        m_fileComparePage = new FileComparePage(this);
        m_fileComparePage->setRecentRecordManager(m_recentRecordManager);
        m_binAnalyzerPage = new BinAnalyzerPage(this, this);
        m_waveformPage = new WaveformPage(this, this);
        m_rtosMonitorPage = new RtosMonitorPage(this, this);
        m_userGuidePage = new UserGuidePage(this);
        m_protocolDecoderPage = new ProtocolDecoderPage(this, this);
        m_serialConsolePage = new SerialConsolePage(this, this);
        m_canBusPage = new CANBusPage(this, this);

        m_pageIndexes.insert(kHomePageId, m_pageStack->addWidget(m_homePage));
        m_pageIndexes.insert(kSerialPageId, m_pageStack->addWidget(m_serialAssistantPage));
        m_pageIndexes.insert(kDataConvertPageId, m_pageStack->addWidget(m_dataConvertPage));
        m_pageIndexes.insert(kFileComparePageId, m_pageStack->addWidget(m_fileComparePage));
        m_pageIndexes.insert(kBinAnalyzerPageId, m_pageStack->addWidget(m_binAnalyzerPage));
        m_pageIndexes.insert(kWaveformPageId, m_pageStack->addWidget(m_waveformPage));
        m_pageIndexes.insert(kRtosMonitorPageId, m_pageStack->addWidget(m_rtosMonitorPage));
        m_pageIndexes.insert(kUserGuidePageId, m_pageStack->addWidget(m_userGuidePage));
        m_pageIndexes.insert(kProtocolDecoderPageId, m_pageStack->addWidget(m_protocolDecoderPage));
        m_pageIndexes.insert(kSerialConsolePageId, m_pageStack->addWidget(m_serialConsolePage));
        m_pageIndexes.insert(kCANBusPageId, m_pageStack->addWidget(m_canBusPage));
    }

    void AppShell::initSignalConnections()
    {
        connect(m_sideNavBar, &SideNavBar::navigationRequested,
                this, &AppShell::switchToPage);

        connect(m_homePage, &HomePage::openSerialAssistantRequested,
                this, [this]()
                { switchToPage(kSerialPageId); });
        connect(m_homePage, &HomePage::openDataConvertRequested,
                this, [this]()
            {
                switchToPage(kDataConvertPageId);
            });
        connect(m_homePage, &HomePage::openFileCompareRequested,
                this, [this]()
            {
                switchToPage(kFileComparePageId);
            });
        connect(m_homePage, &HomePage::openBinAnalyzerRequested,
            this, [this]()
            {
                switchToPage(kBinAnalyzerPageId);
                if (m_binAnalyzerPage != nullptr)
                {
                m_binAnalyzerPage->openFileDialog();
                }
            });
        connect(m_homePage, &HomePage::openWaveformRequested,
            this, [this]()
            {
                switchToPage(kWaveformPageId);
            });
        connect(m_homePage, &HomePage::openRtosMonitorRequested,
            this, [this]()
            {
                switchToPage(kRtosMonitorPageId);
            });
        connect(m_homePage, &HomePage::openUserGuideRequested,
            this, [this]()
            {
                switchToPage(kUserGuidePageId);
            });
        connect(m_homePage, &HomePage::openProtocolDecoderRequested,
            this, [this]()
            {
                switchToPage(kProtocolDecoderPageId);
            });
        connect(m_homePage, &HomePage::openSerialConsoleRequested,
            this, [this]()
            {
                switchToPage(kSerialConsolePageId);
            });
        connect(m_homePage, &HomePage::openCANBusRequested,
            this, [this]()
            {
                switchToPage(kCANBusPageId);
            });
        connect(m_serialAssistantPage, &SerialAssistantPage::systemLogGenerated,
                this, &AppShell::addSystemLog);
        connect(m_serialAssistantPage, &SerialAssistantPage::recentRecordsChanged,
            this, [this]() {
                if (m_homePage != nullptr)
                {
                m_homePage->refreshRecentRecords();
                }
            });
        connect(m_dataConvertPage, &DataConvertPage::statusMessageGenerated,
            this, &AppShell::addSystemLog);
        connect(m_binAnalyzerPage, &BinAnalyzerPage::currentFileChanged,
            this, &AppShell::updateCurrentFileStatus);
        connect(m_binAnalyzerPage, &BinAnalyzerPage::statusMessageGenerated,
            this, &AppShell::addSystemLog);
        connect(m_waveformPage, &WaveformPage::statusMessageGenerated,
            this, &AppShell::addSystemLog);
        connect(m_rtosMonitorPage, &RtosMonitorPage::statusMessageGenerated,
            this, &AppShell::addSystemLog);
        connect(m_protocolDecoderPage, &ProtocolDecoderPage::statusMessageGenerated,
            this, &AppShell::addSystemLog);
        connect(m_serialConsolePage, &SerialConsolePage::statusMessageGenerated,
            this, &AppShell::addSystemLog);
        connect(m_canBusPage, &CANBusPage::statusMessageGenerated,
            this, &AppShell::addSystemLog);
        connect(m_binAnalyzerPage, &BinAnalyzerPage::recentRecordsChanged,
            this, [this]() {
                if (m_homePage != nullptr)
                {
                m_homePage->refreshRecentRecords();
                }
            });
    }

    void AppShell::initRecordingPlayback()
    {
        m_recordStatus = new QLabel(this);
        m_recordStatus->setStyleSheet(QStringLiteral("font-size: 11px; color: #888; padding: 0 4px;"));

        if (m_versionLabel != nullptr)
        {
            statusBar()->removeWidget(m_versionLabel);
        }
        statusBar()->addPermanentWidget(m_recordStatus);
        if (m_versionLabel != nullptr)
        {
            statusBar()->addPermanentWidget(m_versionLabel);
        }

        connect(m_dataBus, &DataBus::dataPublished, this,
                [this](const QString &, const DataPacket &packet)
                {
                    if (m_dataLogger->isRecording()
                        && !packet.metadata.value(QStringLiteral("est.replayed")).toBool())
                    {
                        m_dataLogger->record(packet);
                    }
                });

        connect(m_dataLogger, &DataLogger::recordingProgress, this,
                [this](int frames, qint64 bytes, qint64 ms)
                {
                    const int sec = static_cast<int>(ms / 1000);
                    const QString timeStr = QStringLiteral("%1:%2")
                                                .arg(sec / 60, 2, 10, QLatin1Char('0'))
                                                .arg(sec % 60, 2, 10, QLatin1Char('0'));
                    m_recordStatus->setText(
                        QStringLiteral(" %1 | %2 KB").arg(timeStr).arg(bytes / 1024));
                });
        connect(m_dataLogger, &DataLogger::recordingStopped, this,
                [this](const QString &path, int frames, qint64 bytes)
                {
                    m_pendingRecordingPath = path;
                    m_pendingRecordingFrames = frames;
                    m_pendingRecordingBytes = bytes;
                    updateRecordingActionUi(false);
                    m_recordStatus->setText(
                        tr("待保存 %1 帧 (%2 KB)").arg(frames).arg(bytes / 1024));
                    addSystemLog(tr("录制已停止，点击“保存录制”选择保存位置"));
                });
        connect(m_dataLogger, &DataLogger::recordingError, this,
                [this](const QString &message)
                {
                    updateRecordingActionUi(false);
                    addSystemLog(message);
                });
        updateRecordingActionUi(false);

        m_playbackControl = new PlaybackControl(this);
        m_playbackControl->bindPlayer(m_dataPlayer);

        m_playbackDock = new QDockWidget(tr("回放控制"), this);
        m_playbackDock->setObjectName(QStringLiteral("playbackDock"));
        m_playbackDock->setWidget(m_playbackControl);
        m_playbackDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        m_playbackDock->setTitleBarWidget(new QWidget());
        addDockWidget(Qt::BottomDockWidgetArea, m_playbackDock);
        m_playbackDock->hide();

        connect(m_playbackControl, &PlaybackControl::loadFileRequested,
                this, &AppShell::loadPlaybackFile);
        connect(m_playbackControl, &PlaybackControl::closeRequested,
                this, &AppShell::closePlayback);
        connect(m_dataPlayer, &DataPlayer::playbackError, this, &AppShell::addSystemLog);

        connect(m_dataPlayer, &DataPlayer::packetReady, this,
                [this](const DataPacket &packet)
                {
                    m_dataBus->publish(packet.channel, packet);
                });
    }

    void AppShell::updateSideNavExpansionMode()
    {
        if (m_sideNavBar == nullptr)
        {
            return;
        }

        const bool isFullScreenLayout = isMaximized() || isFullScreen();
        m_sideNavBar->setPinnedExpanded(isFullScreenLayout);
    }

    // ---------------------------------------------------------------------------
    // 录制 & 回放
    // ---------------------------------------------------------------------------

    void AppShell::toggleRecording()
    {
        if (m_dataLogger->isRecording())
        {
            m_dataLogger->stopRecording();
        }
        else
        {
            const QString draftPath = makeDraftRecordingPath();
            if (m_dataLogger->startRecording(draftPath))
            {
                m_pendingRecordingPath.clear();
                m_pendingRecordingFrames = 0;
                m_pendingRecordingBytes = 0;
                updateRecordingActionUi(true);
                m_recordStatus->setText(tr("正在录制"));
                addSystemLog(tr("开始录制"));
            }
            else
            {
                updateRecordingActionUi(false);
                addSystemLog(tr("录制启动失败"));
            }
        }
    }

    void AppShell::updateRecordingActionUi(bool recording)
    {
        if (m_sideNavBar == nullptr)
            return;

        m_sideNavBar->setNavigationItemIcon(
            kRecordActionId,
            moduleIcon(recording ? QStringLiteral("record_stop") : QStringLiteral("record_start")));
        m_sideNavBar->setNavigationItemText(
            kRecordActionId,
            recording ? tr("停止录制") : tr("开始录制"));
    }

    QString AppShell::makeDraftRecordingPath() const
    {
        const QString dirPath = QDir(DataLogger::defaultLogDirectory()).filePath(QStringLiteral("drafts"));
        QDir().mkpath(dirPath);
        const QString timestamp = QDateTime::currentDateTime()
                                      .toString(QStringLiteral("yyyyMMdd_HHmmss"));
        const QString filePath = QDir(dirPath).filePath(QStringLiteral("draft_%1.estlog").arg(timestamp));
        // 确保路径可达（竞态安全：mkpath 已在上方调用）
        QFileInfo fileInfo(filePath);
        if (fileInfo.dir().exists())
        {
            return filePath;
        }
        // 降级到临时目录
        return QDir(QDir::tempPath()).filePath(QStringLiteral("draft_%1.estlog").arg(timestamp));
    }

    void AppShell::saveRecordingAs()
    {
        if (m_dataLogger->isRecording())
        {
            addSystemLog(tr("请先停止录制，再保存日志"));
            return;
        }

        if (m_pendingRecordingPath.isEmpty() || !QFileInfo::exists(m_pendingRecordingPath))
        {
            addSystemLog(tr("没有待保存的录制日志"));
            return;
        }

        const QString defaultName = QStringLiteral("est_%1.estlog")
                                        .arg(QDateTime::currentDateTime()
                                                 .toString(QStringLiteral("yyyyMMdd_HHmmss")));
        const QString savePath = QFileDialog::getSaveFileName(
            this, tr("保存录制日志"), AppPaths::recordingFilePath(defaultName),
            tr("EST 日志 (*.estlog);;所有文件 (*)"));

        if (savePath.isEmpty())
            return;

        if (AppPaths::isDriveCPath(savePath))
        {
            addSystemLog(tr("保存路径不能在 C 盘，请选择软件目录下的数据文件夹"));
            return;
        }

        if (QFileInfo::exists(savePath) && !QFile::remove(savePath))
        {
            addSystemLog(tr("无法覆盖文件: %1").arg(savePath));
            return;
        }

        if (!QFile::copy(m_pendingRecordingPath, savePath))
        {
            addSystemLog(tr("保存录制失败: %1").arg(savePath));
            return;
        }

        QFile::remove(m_pendingRecordingPath);
        // 保存显示用的统计数据（在 clear 之前复制到临时变量）
        const int savedFrames = m_pendingRecordingFrames;
        const qint64 savedBytes = m_pendingRecordingBytes;
        m_pendingRecordingPath.clear();
        m_pendingRecordingFrames = 0;
        m_pendingRecordingBytes = 0;
        m_recordStatus->setText(
            tr("已保存 %1 帧 (%2 KB)").arg(savedFrames).arg(savedBytes / 1024));
        addSystemLog(tr("录制日志已保存: %1").arg(QFileInfo(savePath).fileName()));
    }

    void AppShell::loadPlaybackFile()
    {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("加载回放文件"), AppPaths::recordingsDir(),
            tr("EST 日志 (*.estlog);;所有文件 (*)"));

        if (path.isEmpty())
            return;

        const QFileInfo fileInfo(path);
        if (fileInfo.size() > 100 * 1024 * 1024) {
            addSystemLog(tr("文件过大，无法加载回放"));
            return;
        }

        if (m_dataPlayer->loadFile(path))
        {
            if (m_playbackDock != nullptr)
                m_playbackDock->show();
            m_playbackControl->show();
            addSystemLog(tr("加载回放: %1 (%2 帧)")
                             .arg(QFileInfo(path).fileName())
                             .arg(m_dataPlayer->totalFrames()));
        }
        else
        {
            addSystemLog(tr("加载回放失败: %1").arg(path));
        }
    }

    void AppShell::closePlayback()
    {
        m_dataPlayer->stop();
        m_playbackControl->hide();
        if (m_playbackDock != nullptr)
            m_playbackDock->hide();
        if (m_sideNavBar != nullptr && !m_currentPageId.isEmpty())
            m_sideNavBar->setCurrentId(m_currentPageId);
        addSystemLog(tr("回放已关闭"));
    }

    void AppShell::togglePlaybackPanel()
    {
        if (m_playbackDock == nullptr || m_playbackControl == nullptr)
            return;

        if (m_playbackDock->isVisible())
        {
            closePlayback();
            return;
        }

        m_playbackDock->show();
        m_playbackControl->show();
        addSystemLog(tr("已打开日志回放控制栏"));
    }

    void AppShell::setupStatusBar()
    {
        m_readyLabel = new QLabel(tr("就绪"), this);
        m_serialStateLabel = new QLabel(tr("串口：未连接"), this);
        m_fileStateLabel = new QLabel(tr("文件：无"), this);
        m_txStateLabel = new QLabel(tr("TX：0 B"), this);
        m_rxStateLabel = new QLabel(tr("RX：0 B"), this);
        m_versionLabel = new QLabel(tr("v%1").arg(QApplication::applicationVersion()), this);

        statusBar()->addWidget(m_readyLabel, 1);
        statusBar()->addPermanentWidget(m_serialStateLabel);
        statusBar()->addPermanentWidget(m_fileStateLabel);
        statusBar()->addPermanentWidget(m_txStateLabel);
        statusBar()->addPermanentWidget(m_rxStateLabel);
        statusBar()->addPermanentWidget(m_versionLabel);
        statusBar()->showMessage(tr("就绪"));
    }

    void AppShell::registerPluginViews()
    {
        for (IPlugin *plugin : m_pluginRegistry->allPlugins())
        {
            for (IViewFactory *factory : plugin->viewFactories())
            {
                QDockWidget *dock = new QDockWidget(factory->displayName(), this);
                dock->setObjectName(factory->id());
                dock->setWidget(factory->createWidget(dock));
                addDockWidget(factory->defaultArea(), dock);
                dock->hide();
            }
        }
    }

    void AppShell::switchToPage(const QString &pageId)
    {
        if (pageId == kRecordActionId)
        {
            toggleRecording();
            if (m_sideNavBar != nullptr && !m_currentPageId.isEmpty())
                m_sideNavBar->setCurrentId(m_currentPageId);
            return;
        }

        if (pageId == kSaveRecordingActionId)
        {
            saveRecordingAs();
            if (m_sideNavBar != nullptr && !m_currentPageId.isEmpty())
                m_sideNavBar->setCurrentId(m_currentPageId);
            return;
        }

        if (pageId == kDataLogPageId)
        {
            togglePlaybackPanel();
            if (m_sideNavBar != nullptr && !m_currentPageId.isEmpty())
                m_sideNavBar->setCurrentId(m_currentPageId);
            return;
        }

        const int pageIndex = m_pageIndexes.value(pageId, -1);
        if (pageIndex < 0 || m_pageStack == nullptr)
        {
            return;
        }

        m_pageStack->setCurrentIndex(pageIndex);
        m_currentPageId = pageId;
        // 状态栏已在各页面内部，主窗口不再控制
        if (m_sideNavBar != nullptr)
        {
            m_sideNavBar->setCurrentId(pageId);
        }

    }

    void AppShell::addSystemLog(const QString &text)
    {
        const QString line = makeTimestampedLog(text);
        if (m_homePage != nullptr)
        {
            m_homePage->appendSystemLog(line);
        }

        if (m_readyLabel != nullptr)
        {
            m_readyLabel->setText(text);
        }

        statusBar()->showMessage(text, 3000);
    }

    void AppShell::updateSerialStatus(const QString &text, bool connected)
    {
        if (m_homePage != nullptr)
        {
            m_homePage->setSerialStatus(text, connected);
        }

        if (m_serialStateLabel != nullptr)
        {
            m_serialStateLabel->setText(tr("串口：%1").arg(text));
            m_serialStateLabel->setProperty("status", connected ? "connected" : "disconnected");
            m_serialStateLabel->style()->unpolish(m_serialStateLabel);
            m_serialStateLabel->style()->polish(m_serialStateLabel);
        }
    }

    void AppShell::updateCurrentFileStatus(const QString &text)
    {
        if (m_fileStateLabel != nullptr)
        {
            m_fileStateLabel->setText(text);
        }
        if (m_homePage != nullptr)
        {
            m_homePage->setCurrentFileStatus(text);
        }
    }

    void AppShell::updateTransferStats(qint64 txBytes, qint64 rxBytes)
    {
        if (m_txStateLabel != nullptr)
        {
            m_txStateLabel->setText(tr("TX：%1 B").arg(txBytes));
        }
        if (m_rxStateLabel != nullptr)
        {
            m_rxStateLabel->setText(tr("RX：%1 B").arg(rxBytes));
        }
        if (m_homePage != nullptr)
        {
            m_homePage->setTransferStats(txBytes, rxBytes);
        }
    }

} // namespace est
