cmake_minimum_required(VERSION 3.18)
project(paxos)

set(CMAKE_CXX_STANDARD 20)

find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED system serialization log)
message(${Boost_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})


include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories(util)
include_directories(role)

add_subdirectory(third_party/fmt-9.1.0)
include_directories(third_party/fmt-9.1.0/include)

add_executable(paxos_server
        server_main.cpp
        util/network.cpp
        role/proposer.cpp
        role/acceptor.cpp
        role/learner.cpp
        role/server.cpp
        role/instance.cpp
        role/logger.cpp
        role/config.cpp)
target_link_libraries(paxos_server ${Boost_LIBRARIES} fmt::fmt)


add_executable(paxos_client
        client_main.cpp
        role/client.cpp
        role/config.cpp
        util/network.cpp)
target_link_libraries(paxos_client ${Boost_LIBRARIES} fmt::fmt)
