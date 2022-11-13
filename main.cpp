//
// Created by Jiacheng Wu on 10/6/22.
//

#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>

#include "paxos.h"

int main(int argc, char* argv[]) {
//    auto k = std::move(do_nothing_handler);
//    do_nothing_handler(nullptr, nullptr, {});

    boost::asio::ip::udp::endpoint ends;

    std::cout << sizeof(boost::asio::ip::udp::endpoint) << std::endl;
    return 0;
}