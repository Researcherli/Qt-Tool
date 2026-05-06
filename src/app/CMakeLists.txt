# est_app — 应用可执行文件

set(APP_SOURCES
    main.cpp
    AppShell.cpp
    ../resources/resources.qrc
    pages/BinAnalyzerPage.cpp
    pages/DataConvertPage.cpp
    pages/HomePage.cpp
    pages/SerialAssistantPage.cpp
    widgets/BinAnalyzerWidget.cpp
    widgets/DataConvertWidget.cpp
    widgets/HexViewerWidget.cpp
    widgets/IndustrialStatusCard.cpp
    widgets/QuickCommandPanel.cpp
    widgets/SearchBarWidget.cpp
    widgets/SerialConfigBar.cpp
    widgets/SerialReceiveView.cpp
    widgets/SerialSendPanel.cpp
    widgets/SideNavBar.cpp
    widgets/StringExtractPanel.cpp
)

set(APP_HEADERS
    AppShell.h
    pages/BinAnalyzerPage.h
    pages/DataConvertPage.h
    pages/HomePage.h
    pages/SerialAssistantPage.h
    widgets/BinAnalyzerWidget.h
    widgets/DataConvertWidget.h
    widgets/HexViewerWidget.h
    widgets/IndustrialStatusCard.h
    widgets/QuickCommandPanel.h
    widgets/SearchBarWidget.h
    widgets/SerialConfigBar.h
    widgets/SerialReceiveView.h
    widgets/SerialSendPanel.h
    widgets/SideNavBar.h
    widgets/StringExtractPanel.h
)

add_executable(est_app WIN32 MACOSX_BUNDLE ${APP_SOURCES} ${APP_HEADERS})

target_include_directories(est_app
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(est_app
    PRIVATE
        est_core
)

# 插件目录定义（开发模式）
if(DEFINED EST_PLUGIN_DIR)
    target_compile_definitions(est_app PRIVATE EST_PLUGIN_DIR="${EST_PLUGIN_DIR}")
endif()

# 安装
install(TARGETS est_app
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION .
)

qt_generate_deploy_app_script(
    TARGET est_app
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
