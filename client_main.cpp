//
// Created by Jiacheng Wu on 10/6/22.
//

#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>

#include "client.h"

int main(int argc, char* argv[]) {
//    auto k = std::move(do_nothing_handler);
//    do_nothing_handler(nullptr, nullptr, {});

    if (argc == 1) {
        std::cout << fmt::format("{} {}\n", "program", "port_number");
        return 0;
    }

    std::uint32_t current_id = 0;
    boost::asio::io_context io_context;
    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->load_config();

    PaxosClient client(io_context, std::move(config));


    client.lock(1);
    client.unlock(1);

    return 0;
}