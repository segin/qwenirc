cmake_minimum_required(VERSION 3.20)
project(QwenIRC
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

add_executable(qwenirc
    src/main.cpp
    backend/NetworkManager.cpp
    backend/NetworkManager.h
    backend/IRCChannel.cpp
    backend/IRCChannel.h
    backend/IRCUser.cpp
    backend/IRCUser.h
    backend/IRCMessage.cpp
    backend/IRCMessage.h
    backend/IRCMessageModel.cpp
    backend/IRCMessageModel.h
    frontend/MainWindow.cpp
    frontend/MainWindow.h
    frontend/ChannelTab.cpp
    frontend/ChannelTab.h
    frontend/ChatWidget.cpp
    frontend/ChatWidget.h
    frontend/ServerDialog.cpp
    frontend/ServerDialog.h
)

target_include_directories(qwenirc PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/backend
    ${CMAKE_SOURCE_DIR}/frontend
)

target_link_libraries(qwenirc PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

target_compile_options(qwenirc PRIVATE -Wall -Wextra)

install(TARGETS qwenirc BUNDLE DESTINATION .)
