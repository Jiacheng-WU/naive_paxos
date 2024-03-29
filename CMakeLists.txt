cmake_minimum_required(VERSION 3.18)
project(paxos)

set(CMAKE_CXX_STANDARD 20)

add_compile_definitions(BOOST_LOG_DYN_LINK=1)

find_package(Boost REQUIRED COMPONENTS system serialization log_setup log)
message(${Boost_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories(util)
include_directories(src)

add_executable(paxos_server
        server_main.cpp
        util/network.cpp
        src/proposer.cpp
        src/acceptor.cpp
        src/learner.cpp
        src/server.cpp
        src/instance.cpp
        src/logger.cpp
        src/config.cpp)
target_link_libraries(paxos_server ${Boost_LIBRARIES})


add_executable(paxos_client
        client_main.cpp
        src/client.cpp
        src/config.cpp
        util/network.cpp)
target_link_libraries(paxos_client ${Boost_LIBRARIES})
