# est_app — 应用可执行文件

set(APP_SOURCES
    main.cpp
    AppShell.cpp
    ../resources/resources.qrc
    pages/BinAnalyzerPage.cpp
    pages/DataConvertPage.cpp
    pages/FileComparePage.cpp
    pages/HomePage.cpp
    pages/SerialAssistantPage.cpp
    pages/SerialPortEnumerator.cpp
    widgets/BinAnalyzerWidget.cpp
    widgets/CodeEditor.cpp
    widgets/DataConvertWidget.cpp
    widgets/DiffMinimapWidget.cpp
    widgets/DiffView.cpp
    widgets/HexViewerWidget.cpp
    widgets/IndustrialStatusCard.cpp
    widgets/ModuleIconFactory.cpp
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
    pages/FileComparePage.h
    pages/HomePage.h
    pages/SerialAssistantPage.h
    pages/SerialPortEnumerator.h
    widgets/BinAnalyzerWidget.h
    widgets/CodeEditor.h
    widgets/DataConvertWidget.h
    widgets/DiffMinimapWidget.h
    widgets/DiffView.h
    widgets/HexViewerWidget.h
    widgets/IndustrialStatusCard.h
    widgets/ModuleIconFactory.h
    widgets/QuickCommandPanel.h
    widgets/SearchBarWidget.h
    widgets/SerialConfigBar.h
    widgets/SerialReceiveView.h
    widgets/SerialSendPanel.h
    widgets/SideNavBar.h
    widgets/StringExtractPanel.h
)

set(APP_PLATFORM_RESOURCES)
if(WIN32)
    list(APPEND APP_PLATFORM_RESOURCES app_icon.rc)
endif()

add_executable(est_app WIN32 MACOSX_BUNDLE ${APP_SOURCES} ${APP_HEADERS} ${APP_PLATFORM_RESOURCES})

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

# SerialPortEnumerator.cpp 使用 SEH __try/__except 保护串口枚举，
# 需要 /EHa（异步异常模式）以允许 SEH 与 C++ 对象共存
if(MSVC)
    set_source_files_properties(
        pages/SerialPortEnumerator.cpp
        PROPERTIES COMPILE_FLAGS "/EHa"
    )
endif()

# Windows 开发运行时部署：让 Qt Creator 直接运行 build/bin/est_app.exe 时也能找到 qwindows.dll。
if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET est_app POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    "$<IF:$<CONFIG:Debug>,--debug,--release>"
                    --compiler-runtime
                    "$<TARGET_FILE:est_app>"
            COMMENT "Deploying Qt runtime files for est_app"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt was not found; est_app may fail to start without Qt platform plugins.")
    endif()
endif()

# 安装
install(TARGETS est_app
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION .
)

# 复制 qt.conf 到输出目录，帮助 Qt 定位插件和资源配置
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../resources/qt.conf
    ${CMAKE_BINARY_DIR}/bin/qt.conf
    COPYONLY
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../resources/qt.conf
    DESTINATION bin
)

qt_generate_deploy_app_script(
    TARGET est_app
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
