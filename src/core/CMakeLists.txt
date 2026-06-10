# est_core — 核心静态库

set(CORE_SOURCES
    # databus
    databus/DataBus.cpp
    # plugin
    plugin/PluginRegistry.cpp
    plugin/PluginLoader.cpp
    # services
    services/BinFileLoader.cpp
    services/BinSearchService.cpp
    services/AppPaths.cpp
    services/ByteChecksumService.cpp
    services/ByteDataInspectorService.cpp
    services/ByteFormatService.cpp
    services/DataConvertService.cpp
    services/DataLogger.cpp
    services/ProtocolDecoder.cpp
    services/ProtocolTemplate.cpp
    services/RecentRecordManager.cpp
    services/RtosTaskParser.cpp
    services/SlcanCodec.cpp
    services/StringExtractor.cpp
    services/EncodingUtil.cpp
    # utils/algo
    FileCompareEngine.cpp
    # transport
    transport/TransportRegistry.cpp
    transport/SerialTransport.cpp
)

set(CORE_HEADERS
    # databus
    databus/DataPacket.h
    databus/SubscriptionHandle.h
    databus/DataBus.h
    # plugin
    plugin/IPlugin.h
    plugin/IViewFactory.h
    plugin/ICore.h
    plugin/PluginRegistry.h
    plugin/PluginLoader.h
    # services
    services/BinFileLoader.h
    services/BinSearchService.h
    services/AppPaths.h
    services/ByteChecksumService.h
    services/ByteDataInspectorService.h
    services/ByteFormatService.h
    services/DataConvertService.h
    services/DataLogger.h
    services/ProtocolDecoder.h
    services/ProtocolTemplate.h
    services/RecentRecordManager.h
    services/RtosTaskParser.h
    services/SlcanCodec.h
    services/StringExtractor.h
    services/EncodingUtil.h
    # utils/algo
    FileCompareEngine.h
    # transport
    transport/ITransport.h
    transport/SerialTransport.h
    transport/TransportRegistry.h
)

add_library(est_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(est_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(est_core
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        Qt6::SerialPort
)
