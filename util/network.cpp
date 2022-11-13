//
// Created by Jiacheng Wu on 10/8/22.
//

#include "network.h"


uint64_t get_uint64_from_udp_ipv4_endpoint(std::unique_ptr<boost::asio::ip::udp::endpoint>& endpoint) {
    boost::asio::ip::address_v4::uint_type ip = endpoint->address().to_v4().to_uint();
    uint32_t port = endpoint->port();
    uint64_t result = uint64_t(ip) << 32 | port;
    return result;
}

std::unique_ptr<boost::asio::ip::udp::endpoint> get_udp_ipv4_endpoint_from_uint64_t(uint64_t compressed) {
    boost::asio::ip::address_v4::uint_type ip = compressed >> 32;
    uint32_t port = compressed & 0xFFFFFFFF;
    return std::make_unique<boost::asio::ip::udp::endpoint>(boost::asio::ip::make_address_v4(ip), port);
}