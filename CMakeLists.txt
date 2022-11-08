cmake_minimum_required(VERSION 3.18)
project(paxos)

set(CMAKE_CXX_STANDARD 20)

find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(Boost REQUIRED system)
message(${Boost_INCLUDE_DIRS})
include_directories(${gRPC_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)

include_directories(${Protobuf_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_library(myproto proto/paxos.proto)
target_link_libraries(myproto
        PUBLIC
        protobuf::libprotobuf
        gRPC::grpc
        gRPC::grpc++
        )
protobuf_generate(TARGET myproto LANGUAGE cpp)
protobuf_generate(TARGET myproto LANGUAGE grpc GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}")

include_directories(util)
include_directories(role)


add_executable(paxos main.cpp paxos.cpp paxos.h util/network.cpp util/network.h
        role/proposer.cpp role/proposer.h role/acceptor.cpp role/acceptor.h role/learner.cpp role/learner.h
        role/paxos.cpp role/paxos.h role/Instance.cpp role/Instance.h)
target_link_libraries(paxos gRPC::grpc++ ${Boost_LIBRARIES} myproto)
