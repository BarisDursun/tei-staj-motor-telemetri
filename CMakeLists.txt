#Poject settings
cmake_minimum_required(VERSION 3.16)
project(EngineTelemetry LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Quick Test SerialPort)


#motor telemetri mantığı Static Library de
add_library(enginecore STATIC        #data process kısmı
    enginemodel.cpp
    enginemodel.h
    engine_core.h
    embeddedtempsensor.cpp
    embeddedtempsensor.h
)
target_link_libraries(enginecore PUBLIC Qt5::Core Qt5::SerialPort)
target_include_directories(enginecore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})   #Ek modüllerin .h dosyaları bu dizide olmalı


#Projenin asıl .exe file
add_executable(EngineTelemetry
    main.cpp
    resources.qrc
)
#Uygulamaya yetenekler yüklenir
target_link_libraries(EngineTelemetry PRIVATE
    enginecore        #Core lib.
    Qt5::Core         #UI lib.
    Qt5::Quick
)

enable_testing()
add_subdirectory(tests)
