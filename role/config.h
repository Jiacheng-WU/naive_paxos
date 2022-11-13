//
// Created by Jiacheng Wu on 11/11/22.
//

#ifndef PAXOS_CONFIG_H
#define PAXOS_CONFIG_H

#include <cstdint>
#include <map>
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <filesystem>
struct Config {
    using server_id_t = std::uint32_t;
    using server_addr_t = boost::asio::ip::udp::endpoint;
    std::uint32_t number_of_nodes = 10;
    std::uint32_t current_id = 0;

    std::map<server_id_t, server_addr_t> server_id_to_addr_map = {
            {0, server_addr_t(boost::asio::ip::udp::v4(), 8080)},
    };

    std::uint32_t get_number_of_nodes() const {
        return number_of_nodes;
    };

    std::uint32_t get_current_id() const {
        return current_id;
    }
    bool load_config(std::filesystem::path json_file);

    std::unique_ptr<server_addr_t> get_addr_by_id(server_id_t server_id) {
        return std::make_unique<server_addr_t>(server_id_to_addr_map[server_id]);
    }
};


#endif //PAXOS_CONFIG_H
