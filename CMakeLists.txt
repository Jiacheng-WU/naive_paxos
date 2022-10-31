cmake_minimum_required(VERSION 3.18)
project(paxos)

set(CMAKE_CXX_STANDARD 20)

find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED system)
message(${Boost_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})


include_directories(util)
include_directories(role)

add_executable(paxos main.cpp paxos.cpp paxos.h util/network.cpp util/network.h role/proposer.cpp role/proposer.h role/acceptor.cpp role/acceptor.h role/learner.cpp role/learner.h)
target_link_libraries(paxos gRPC::grpc++ ${Boost_LIBRARIES})
