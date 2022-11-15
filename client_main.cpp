//
// Created by Jiacheng Wu on 10/6/22.
//

#include <iostream>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim_all.hpp>

#include "client.hpp"
#include <fstream>

#include "magic_enum.hpp"
#include <chrono>
#include <thread>

int main(int argc, char* argv[]) {

//
//    std::uint32_t current_id = 0;

    std::string config_filepath_str = "../config.json";
    if (argc == 1) {
        BOOST_LOG_TRIVIAL(warning) << fmt::format("You could specify config file by add the path after program!\n");
    } else if (argc == 2) {
        config_filepath_str = std::string(argv[1]);
    }
    std::unique_ptr<Config> config = std::make_unique<Config>();
    config->load_config(std::filesystem::path(config_filepath_str));

    PaxosClient client(std::move(config));
    boost::system::error_code error;

    fmt::print("lock(1)\t: response {}\n" , magic_enum::enum_name(client.lock(1)));
    fmt::print("lock(1)\t: response {}\n" , magic_enum::enum_name(client.lock(1)));

    fmt::print("lock(2)\t: response {}\n" , magic_enum::enum_name(client.lock(2)));
    fmt::print("lock(2)\t: response {}\n" , magic_enum::enum_name(client.lock(2)));

    std::this_thread::sleep_for(std::chrono::seconds(10));

    fmt::print("unlock(2)\t: response {}\n" , magic_enum::enum_name(client.unlock(2)));
    fmt::print("unlock(2)\t: response {}\n" , magic_enum::enum_name(client.unlock(2)));

    fmt::print("unlock(1)\t: response {}\n" , magic_enum::enum_name(client.unlock(1)));
    fmt::print("unlock(1)\t: response {}\n" , magic_enum::enum_name(client.unlock(1)));

    return 0;
}
