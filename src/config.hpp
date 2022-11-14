//
// Created by Jiacheng Wu on 11/11/22.
//

#ifndef PAXOS_CONFIG_HPP
#define PAXOS_CONFIG_HPP

#include <cstdint>
#include <map>
#include <boost/asio.hpp>
#include <boost/json.hpp>
#include <filesystem>
struct Config {
    using server_id_t = std::uint32_t;
    using server_addr_t = boost::asio::ip::udp::endpoint;
    std::uint32_t number_of_nodes = 0;

    std::uint32_t after_prepare_milliseconds = 2000;
    std::uint32_t after_accept_milliseconds = 2000;

    std::uint32_t client_retry_milliseconds = 4000;

    std::filesystem::path get_acceptor_file_path(server_id_t server_id);
    std::filesystem::path get_learner_file_path(server_id_t server_id);

    std::map<server_id_t, server_addr_t> server_id_to_addr_map;

    std::uint32_t get_number_of_nodes() const {
        return number_of_nodes;
    };


    bool load_config();

    bool load_config(std::filesystem::path json_file);

    std::unique_ptr<server_addr_t> get_addr_by_id(server_id_t server_id) {
        return std::make_unique<server_addr_t>(server_id_to_addr_map[server_id]);
    }
};


#endif //PAXOS_CONFIG_HPP
