#include "AppShell.h"

#include "databus/DataBus.h"
#include "pages/BinAnalyzerPage.h"
#include "pages/DataConvertPage.h"
#include "pages/HomePage.h"
#include "pages/SerialAssistantPage.h"
#include "plugin/IViewFactory.h"
#include "plugin/PluginLoader.h"
#include "plugin/PluginRegistry.h"
#include "services/RecentRecordManager.h"
#include "transport/SerialTransport.h"
#include "transport/TransportRegistry.h"
#include "widgets/SideNavBar.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
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
        const QString kBinAnalyzerPageId = QStringLiteral("bin_analyzer");

        QString makeTimestampedLog(const QString &text)
        {
            return QStringLiteral("[%1] %2")
                .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")), text);
        }

        QIcon standardIcon(QStyle::StandardPixmap pixmap)
        {
            return QApplication::style()->standardIcon(pixmap);
        }

    } // namespace

    AppShell::AppShell(QWidget *parent)
        : QMainWindow(parent), m_dataBus(new DataBus(this)), m_pluginRegistry(new PluginRegistry(this)), m_transportRegistry(new TransportRegistry(this)), m_recentRecordManager(new RecentRecordManager(this))
    {
        setWindowTitle(tr("嵌入式工具"));
        resize(1360, 860);
        setMinimumSize(1180, 720);

        m_transportRegistry->registerFactory(QStringLiteral("serial"), []()
                                             { return new SerialTransport(); });

        setupWorkspace();
        setupStatusBar();
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

    void AppShell::setupToolBar()
    {
    }

    void AppShell::setupWorkspace()
    {
        QWidget *central = new QWidget(this);
        central->setObjectName(QStringLiteral("workspaceRoot"));

        auto *layout = new QHBoxLayout(central);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        m_sideNavBar = new SideNavBar(central);
        m_sideNavBar->addNavigationItem(kHomePageId, tr("首页"),
                                         standardIcon(QStyle::SP_DesktopIcon));
        m_sideNavBar->addNavigationItem(kSerialPageId, tr("串口工具"),
                                         standardIcon(QStyle::SP_DriveNetIcon));
        m_sideNavBar->addNavigationItem(kDataConvertPageId, tr("数据转换"),
                                         standardIcon(QStyle::SP_FileDialogDetailedView));
        m_sideNavBar->addNavigationItem(kBinAnalyzerPageId, tr("BIN 文件分析"),
                                         standardIcon(QStyle::SP_FileIcon));

        m_pageStack = new QStackedWidget(central);
        m_pageStack->setObjectName(QStringLiteral("pageStack"));

        m_homePage = new HomePage(this, central);
        m_serialAssistantPage = new SerialAssistantPage(this, central);
        m_dataConvertPage = new DataConvertPage(central);
        m_binAnalyzerPage = new BinAnalyzerPage(this, central);

        m_pageIndexes.insert(kHomePageId, m_pageStack->addWidget(m_homePage));
        m_pageIndexes.insert(kSerialPageId, m_pageStack->addWidget(m_serialAssistantPage));
        m_pageIndexes.insert(kDataConvertPageId, m_pageStack->addWidget(m_dataConvertPage));
        m_pageIndexes.insert(kBinAnalyzerPageId, m_pageStack->addWidget(m_binAnalyzerPage));

        layout->addWidget(m_sideNavBar);
        layout->addWidget(m_pageStack, 1);

        setCentralWidget(central);

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
        connect(m_homePage, &HomePage::openBinAnalyzerRequested,
            this, [this]()
            {
                switchToPage(kBinAnalyzerPageId);
                if (m_binAnalyzerPage != nullptr)
                {
                m_binAnalyzerPage->openFileDialog();
                }
            });
        connect(m_serialAssistantPage, &SerialAssistantPage::serialStatusChanged,
                this, &AppShell::updateSerialStatus);
        connect(m_serialAssistantPage, &SerialAssistantPage::systemLogGenerated,
                this, &AppShell::addSystemLog);
        connect(m_serialAssistantPage, &SerialAssistantPage::transferStatsChanged,
            this, &AppShell::updateTransferStats);
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
        connect(m_binAnalyzerPage, &BinAnalyzerPage::recentRecordsChanged,
            this, [this]() {
                if (m_homePage != nullptr)
                {
                m_homePage->refreshRecentRecords();
                }
            });

        switchToPage(kHomePageId);
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
        const int pageIndex = m_pageIndexes.value(pageId, -1);
        if (pageIndex < 0 || m_pageStack == nullptr)
        {
            return;
        }

        m_pageStack->setCurrentIndex(pageIndex);
        statusBar()->setVisible(pageId == kSerialPageId);
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
