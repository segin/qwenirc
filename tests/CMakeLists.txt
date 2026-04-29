cmake_minimum_required(VERSION 3.20)

find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(irc_parser_tests
    test_irc_parser.cpp
    ../backend/NetworkManager.cpp
    ../backend/IRCChannel.cpp
    ../backend/IRCUser.cpp
    ../backend/IRCMessage.cpp
    ../backend/IRCMessageModel.cpp
    ../backend/NetworkManager.h
    ../backend/IRCChannel.h
    ../backend/IRCUser.h
    ../backend/IRCMessage.h
    ../backend/IRCMessageModel.h
)

add_executable(irc_user_model_tests
    test_irc_user_model.cpp
    ../backend/NetworkManager.cpp
    ../backend/IRCChannel.cpp
    ../backend/IRCUser.cpp
    ../backend/IRCMessage.cpp
    ../backend/IRCMessageModel.cpp
    ../backend/NetworkManager.h
    ../backend/IRCChannel.h
    ../backend/IRCUser.h
    ../backend/IRCMessage.h
    ../backend/IRCMessageModel.h
)

target_include_directories(irc_parser_tests PRIVATE ../backend)
target_include_directories(irc_user_model_tests PRIVATE ../backend)

target_link_libraries(irc_parser_tests PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Test
)

target_link_libraries(irc_user_model_tests PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Test
)

add_test(NAME irc_parser_tests COMMAND irc_parser_tests)
add_test(NAME irc_user_model_tests COMMAND irc_user_model_tests)
