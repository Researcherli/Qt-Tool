add_executable(est_app_widget_tests
    app_widget_tests.cpp
    ../src/app/pages/BinAnalyzerPage.cpp
    ../src/app/pages/DataConvertPage.cpp
    ../src/app/pages/RtosMonitorPage.cpp
    ../src/app/pages/SerialAssistantPage.cpp
    ../src/app/pages/SerialPortEnumerator.cpp
    ../src/app/pages/WaveformPage.cpp
    ../src/app/widgets/QuickCommandPanel.cpp
    ../src/app/widgets/DataConvertWidget.cpp
    ../src/app/widgets/BinAnalyzerWidget.cpp
    ../src/app/widgets/HexViewerWidget.cpp
    ../src/app/widgets/SearchBarWidget.cpp
    ../src/app/widgets/StringExtractPanel.cpp
    ../src/app/widgets/SerialConfigBar.cpp
    ../src/app/widgets/SerialReceiveView.cpp
    ../src/app/widgets/SerialSendPanel.cpp
    ../src/app/widgets/WaveformChartWidget.cpp
    ../src/core/services/BinFileLoader.cpp
    ../src/core/services/BinSearchService.cpp
    ../src/core/services/ByteChecksumService.cpp
    ../src/core/services/ByteDataInspectorService.cpp
    ../src/core/services/ByteFormatService.cpp
    ../src/core/services/ProtocolDecoder.cpp
    ../src/core/services/ProtocolTemplate.cpp
    ../src/core/services/StringExtractor.cpp
    ../src/core/databus/DataBus.cpp
    ../src/core/services/DataConvertService.cpp
    ../src/core/services/RecentRecordManager.cpp
    ../src/core/services/RtosTaskParser.cpp
    ../src/core/services/SlcanCodec.cpp
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
        Qt6::Charts
)

if(MSVC)
    set_source_files_properties(
        ../src/app/pages/SerialPortEnumerator.cpp
        PROPERTIES COMPILE_FLAGS "/EHa"
    )
endif()

add_test(NAME est_app_widget_tests COMMAND est_app_widget_tests)

add_executable(est_file_compare_engine_tests
    file_compare_engine_tests.cpp
    ../src/core/FileCompareEngine.cpp
)

target_include_directories(est_file_compare_engine_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(est_file_compare_engine_tests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME est_file_compare_engine_tests COMMAND est_file_compare_engine_tests)

add_executable(est_file_compare_page_tests
    file_compare_page_tests.cpp
    ../src/app/pages/FileComparePage.cpp
    ../src/app/widgets/CodeEditor.cpp
    ../src/app/widgets/DiffMinimapWidget.cpp
    ../src/app/widgets/IndustrialStatusCard.cpp
    ../src/core/FileCompareEngine.cpp
    ../src/core/services/RecentRecordManager.cpp
)

target_include_directories(est_file_compare_page_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/app
        ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(est_file_compare_page_tests
    PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Test
)

add_test(NAME est_file_compare_page_tests COMMAND est_file_compare_page_tests)

add_executable(est_data_logger_tests
    data_logger_tests.cpp
    ../src/core/services/DataLogger.cpp
    ../src/core/databus/DataBus.cpp
)

target_include_directories(est_data_logger_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(est_data_logger_tests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME est_data_logger_tests COMMAND est_data_logger_tests)

if(WIN32)
    get_target_property(QT_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    set_tests_properties(est_data_logger_tests PROPERTIES
        ENVIRONMENT "PATH=${QT_BIN_DIR}\\;$ENV{PATH}"
    )
endif()

add_executable(est_serial_transport_tests
    serial_transport_tests.cpp
    ../src/core/transport/SerialTransport.cpp
    ../src/core/transport/TransportRegistry.cpp
    ../src/core/services/DataLogger.cpp
    ../src/core/databus/DataBus.cpp
)

target_include_directories(est_serial_transport_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(est_serial_transport_tests
    PRIVATE
        Qt6::Core
        Qt6::SerialPort
        Qt6::Test
)

add_test(NAME est_serial_transport_tests COMMAND est_serial_transport_tests)
