cmake_minimum_required(VERSION 3.16)
project(mp-geosight LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick Network)

add_executable(mp-geosight
    main.cpp
    src/core/NetworkManager.cpp
    src/core/GtiProtocolParser.cpp
    src/core/DataStore.cpp
    src/model/ChartConfigManager.cpp
    src/model/SensorCatalog.cpp
    src/ui/GeoChartItem.cpp
    src/ui/TimeAxisItem.cpp
    qml.qrc
)

target_include_directories(mp-geosight PRIVATE src)
target_link_libraries(mp-geosight PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Network)

if(WIN32)
    target_sources(mp-geosight PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/mp-geosight.rc)
endif()
