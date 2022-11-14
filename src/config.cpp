//
// Created by Jiacheng Wu on 11/12/22.
//

#include "config.hpp"
#include "fmt/core.h"
bool Config::load_config(std::filesystem::path json_file) {
    assert("Unsupported");
    return false;
}

bool Config::load_config() {
    server_id_to_addr_map = {
            {0, server_addr_t(boost::asio::ip::udp::v4(), 8080)},
            {1, server_addr_t(boost::asio::ip::udp::v4(), 8081)},
            {2, server_addr_t(boost::asio::ip::udp::v4(), 8082)},
//            {3, server_addr_t(boost::asio::ip::udp::v4(), 8083)},
//            {4, server_addr_t(boost::asio::ip::udp::v4(), 8084)},
    };
    number_of_nodes = server_id_to_addr_map.size();
    return true;
}

std::filesystem::path Config::get_learner_file_path(Config::server_id_t server_id) {
    return std::filesystem::path(fmt::format("./server{}_learner.log", server_id));
}

std::filesystem::path Config::get_acceptor_file_path(Config::server_id_t server_id) {
    return std::filesystem::path(fmt::format("./server{}_acceptor.log", server_id));
}
