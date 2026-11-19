cmake_minimum_required(VERSION 3.16)
project(WorkBuddy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_executable(WorkBuddy
    src/main.cpp
    src/mainwindow.h
    src/mainwindow.cpp
    src/settingsdialog.h
    src/settingsdialog.cpp
    src/skillmanager.h
    src/skillmanager.cpp
    src/aiclient.h
    src/aiclient.cpp
)

target_link_libraries(WorkBuddy PRIVATE Qt6::Widgets Qt6::Network shell32)
