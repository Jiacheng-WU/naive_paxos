//
// Created by Jiacheng Wu on 10/6/22.
//

#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>

#include "client.hpp"
#include <fstream>
int main(int argc, char* argv[]) {
//    auto k = std::move(do_nothing_handler);
//    do_nothing_handler(nullptr, nullptr, {});
//    std::fstream out;
//    if (!std::filesystem::exists("file.log")) {
//        out.open("file.log", std::ios::out | std::ios::binary);
//        out.close();
//    }
//    out.open("file.log", std::ios::out | std::ios::in | std::ios::binary);
//    std::cout << std::filesystem::file_size("file.log") << "\n";
//    out.seekp(0);
//    char k[4] = {};
//    out.read(k, 3);
//    out.flush();
//    std::cout << k << "\n";
//    out.close();
//
//    return 0;

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