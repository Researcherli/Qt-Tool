add_executable(est_app_widget_tests
    app_widget_tests.cpp
    ../src/app/pages/SerialAssistantPage.cpp
    ../src/app/widgets/QuickCommandPanel.cpp
    ../src/app/widgets/DataConvertWidget.cpp
    ../src/app/widgets/SerialConfigBar.cpp
    ../src/app/widgets/SerialReceiveView.cpp
    ../src/app/widgets/SerialSendPanel.cpp
    ../src/core/services/ByteChecksumService.cpp
    ../src/core/services/ByteDataInspectorService.cpp
    ../src/core/services/ByteFormatService.cpp
    ../src/core/databus/DataBus.cpp
    ../src/core/services/DataConvertService.cpp
    ../src/core/services/RecentRecordManager.cpp
    ../src/core/transport/ITransport.h
    ../src/core/transport/TransportRegistry.cpp
)

target_include_directories(est_app_widget_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/app
        ${CMAKE_SOURCE_DIR}/src/core
)

target_compile_definitions(est_app_widget_tests
    PRIVATE
        EST_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(est_app_widget_tests
    PRIVATE
        Qt6::Core
        Qt6::SerialPort
        Qt6::Widgets
        Qt6::Test
)

add_test(NAME est_app_widget_tests COMMAND est_app_widget_tests)
