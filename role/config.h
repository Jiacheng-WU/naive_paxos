//
// Created by Jiacheng Wu on 11/11/22.
//

#ifndef PAXOS_CONFIG_H
#define PAXOS_CONFIG_H

#include <cstdint>
#include <map>
#include <boost/asio.hpp>
struct Config {
    using server_id_t = std::uint32_t;
    using server_addr_t = boost::asio::ip::udp::endpoint;
    inline constinit const static server_id_t number_of_nodes = 10;
    inline static std::map<server_id_t, server_addr_t> server_id_to_addr_map = {
            {0, server_addr_t(boost::asio::ip::udp::v4(), 8080)},
    };

    static std::unique_ptr<server_addr_t> get_addr_by_id(server_id_t server_id) {
        return std::make_unique<server_addr_t>(server_id_to_addr_map[server_id]);
    }


};

#endif //PAXOS_CONFIG_H
