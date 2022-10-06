cmake_minimum_required(VERSION 3.18)
project(paxos)

set(CMAKE_CXX_STANDARD 20)
# include_directories(boost_string)
add_executable(paxos paxos.cpp)
target_link_libraries(paxos)
