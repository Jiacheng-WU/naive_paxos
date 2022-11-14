//
// Created by Jiacheng Wu on 10/6/22.
//

#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>

#include "client.hpp"

int main(int argc, char* argv[]) {
//    auto k = std::move(do_nothing_handler);
//    do_nothing_handler(nullptr, nullptr, {});

    std::uint32_t current_id = 0;
    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->load_config();

    PaxosClient client(std::move(config));
    boost::system::error_code error;
//    auto len = client.receive(boost::asio::buffer(client.in_message), std::chrono::milliseconds(1000), error);
//    if (error) {
//        std::cout << error.what() << std::endl;
//    }

    client.lock(1);
    client.unlock(1);

    return 0;
}