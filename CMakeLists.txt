cmake_minimum_required(VERSION 3.20)
project(QwenIRC
    VERSION 0.1.0
    LANGUAGES CXX
)

enable_testing()

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)

find_package(OpenSSL QUIET)
option(QWENIRC_TLS "Build with IRC TLS support (requires OpenSSL)" ON)
if(QWENIRC_TLS)
    if(OPENSSL_FOUND)
        set(HAVE_TLS ON)
    else()
        message(STATUS "QWENIRC_TLS enabled but OpenSSL not found; TLS disabled")
        set(HAVE_TLS OFF)
    endif()
endif()

add_executable(qwenirc
    src/main.cpp
    backend/NetworkManager.cpp
    backend/NetworkManager.h
    backend/IRCChannel.cpp
    backend/IRCChannel.h
    backend/IRCUser.h
    backend/IRCUser.cpp
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

if(HAVE_TLS)
    target_link_libraries(qwenirc PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    option(QWENIRC_WALL "Enable -Wall -Wextra warnings" ON)
    if(QWENIRC_WALL)
        target_compile_options(qwenirc PRIVATE -Wall -Wextra)
        if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            target_compile_options(qwenirc PRIVATE -Werror)
        endif()
    endif()
endif()

add_subdirectory(tests)

install(TARGETS qwenirc
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)
