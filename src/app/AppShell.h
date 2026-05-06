#ifndef EST_APPSHELL_H
#define EST_APPSHELL_H

#include "plugin/ICore.h"

#include <QMainWindow>
#include <QHash>

class QLabel;
class QStackedWidget;

namespace est
{

    class BinAnalyzerPage;
    class DataConvertPage;
    class DataBus;
    class HomePage;
    class PluginRegistry;
    class PluginLoader;
    class RecentRecordManager;
    class SerialAssistantPage;
    class SideNavBar;
    class TransportRegistry;

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

    private:
        void setupStatusBar();
        void setupToolBar();
        void setupWorkspace();
        void registerPluginViews();
        void switchToPage(const QString &pageId);
        void addSystemLog(const QString &text);
        void updateSerialStatus(const QString &text, bool connected);
        void updateCurrentPageTitle(const QString &title);
        void updateCurrentFileStatus(const QString &text);
        void updateTransferStats(qint64 txBytes, qint64 rxBytes);

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
        BinAnalyzerPage *m_binAnalyzerPage = nullptr;
        QLabel *m_readyLabel = nullptr;
        QLabel *m_serialStateLabel = nullptr;
        QLabel *m_fileStateLabel = nullptr;
        QLabel *m_txStateLabel = nullptr;
        QLabel *m_rxStateLabel = nullptr;
        QLabel *m_versionLabel = nullptr;
        QLabel *m_titleLabel = nullptr;
        QLabel *m_pageTitleLabel = nullptr;
        QHash<QString, int> m_pageIndexes;
    };

} // namespace est

#endif // EST_APPSHELL_H
