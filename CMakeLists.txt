cmake_minimum_required(VERSION 3.18)
project(paxos)

set(CMAKE_CXX_STANDARD 20)

find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED system serialization)
message(${Boost_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})


include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories(util)
include_directories(role)

add_subdirectory(third_party/fmt-9.1.0)

add_executable(paxos main.cpp util/network.cpp util/network.h
        role/proposer.cpp role/proposer.h role/acceptor.cpp role/acceptor.h role/learner.cpp role/learner.h
        role/paxos.cpp role/paxos.h role/instance.cpp role/instance.h role/message.h util/sync.h role/logger.cpp role/logger.h role/config.h)
target_link_libraries(paxos ${Boost_LIBRARIES} fmt::fmt)
