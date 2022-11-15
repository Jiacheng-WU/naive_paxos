//
// Created by Jiacheng Wu on 10/8/22.
//

#ifndef PAXOS_NETWORK_HPP
#define PAXOS_NETWORK_HPP

#include <boost/asio.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <iostream>
#include "message.hpp"
#include "fmt/core.h"
#include <random>

inline constexpr const uint64_t unused_udp_ipv4_number = ~0ull;
uint64_t get_uint64_from_udp_ipv4_endpoint(std::unique_ptr<boost::asio::ip::udp::endpoint>& endpoint);
std::unique_ptr<boost::asio::ip::udp::endpoint> get_udp_ipv4_endpoint_from_uint64_t(uint64_t compressed);


std::uint32_t get_random_number(std::uint32_t begin, std::uint32_t end);

struct asio_handler_paras {
    boost::system::error_code ec;
    std::size_t length;
};

using Handler = std::function<void(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras)>;
inline Handler do_nothing_handler = [](std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras){};

class Connection {
  public:
    /// Constructor.
    Connection(boost::asio::ip::udp::socket& socket_)
    : socket_(socket_) {}

    /// Get the underlying socket. Used for making a Connection or for accepting
    /// an incoming Connection.
    boost::asio::ip::udp::socket& socket()
    {
        return socket_;
    }

    // In UDP, we do not need to consider whether the datagram is sent partially
    // the datagram only could be sent entirely or just dropped

    void resend_if_do_send_failed(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, Handler handler) {
        struct inner_retry {
            std::uint32_t times;
            Connection* connect;
            Handler handler;
            inner_retry(std::uint32_t times, Connection* connect, Handler handler):
                times(times), connect(connect), handler(std::move(handler)) {};
            void operator()(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras) {
                if (this->times > 0) {
                    connect->do_send(std::move(m_p), std::move(endpoint),
                                     inner_retry(this->times - 1, this->connect, std::move(handler)));
                    return;
                } else {
                    // Handle errors
                    BOOST_LOG_TRIVIAL(debug) << fmt::format("Failed to receive {}:{} with error {}\n",
                                                            endpoint->address().to_string(),
                                                            endpoint->port(),
                                                            paras.ec.message());
                    handler(std::move(m_p), std::move(endpoint), paras);
                    // Won't resend But still won't failed
                }
            }
        };
        do_send(std::move(m_p), std::move(endpoint), inner_retry(SEND_RETRY_TIMES - 1, this, std::move(handler)));
    }

    void do_send(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, Handler handler);

    void do_receive(std::unique_ptr<Message> m_p, Handler handler);
    boost::asio::ip::udp::socket& socket_;

    void set_send_retry_time(std::uint32_t retry_times) {
        SEND_RETRY_TIMES = retry_times;
    }

    std::uint32_t get_send_retry_time() {
        return SEND_RETRY_TIMES;
    }

  private:
    std::uint32_t SEND_RETRY_TIMES = 1;
};


#endif //PAXOS_NETWORK_HPP
