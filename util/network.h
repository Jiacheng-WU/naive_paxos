//
// Created by Jiacheng Wu on 10/8/22.
//

#ifndef PAXOS_NETWORK_H
#define PAXOS_NETWORK_H

#include <boost/asio.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/stream.hpp>
#include <iostream>
#include "message.h"
#include "fmt/core.h"


inline constexpr const uint64_t unused_udp_ipv4_number = ~0ull;
uint64_t get_uint64_from_udp_ipv4_endpoint(std::unique_ptr<boost::asio::ip::udp::endpoint>& endpoint);
std::unique_ptr<boost::asio::ip::udp::endpoint> get_udp_ipv4_endpoint_from_uint64_t(uint64_t compressed);

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

    const std::uint32_t SEND_RETRY_TIMES = 1;

    void resend_if_do_send_failed(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, Handler handler) {
        struct inner_retry {
            std::uint32_t times;
            Connection* connect;
            Handler handler;
            inner_retry(std::uint32_t times, Connection* connect, Handler handler):
                times(times), connect(connect), handler(std::move(handler)) {};
            void operator()(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras) {
                if (paras.ec) {
                    handler(std::move(m_p), std::move(endpoint), paras);;
                } else if (this->times > 0) {
                    connect->do_send(std::move(m_p), std::move(endpoint),
                                     inner_retry(this->times - 1, this->connect, std::move(handler)));
                    return;
                } else {
                        // Handle errors
                        std::cerr << fmt::format("Failed to send with error {}", paras.ec.message());
                        // Won't resend But still won't failed
                }
            }
        };
        do_send(std::move(m_p), std::move(endpoint), inner_retry(SEND_RETRY_TIMES - 1, this, std::move(handler)));
    }

    void do_send(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, Handler handler)
    {
        std::unique_ptr<MessageBuffer> out_buf = std::make_unique<MessageBuffer>();
        m_p->serialize_to(out_buf->buffer);
        socket_.async_send_to(boost::asio::buffer(out_buf->buffer), *endpoint,
                                 [this, m_p = std::move(m_p), endpoint = std::move(endpoint), handler = std::move(handler), out_buf = std::move(out_buf)]
                                 (boost::system::error_code ec, std::size_t length) mutable -> void {
                                    assert(length == Message::size());
                                    // We should first release our buffer to avoid recursive memory leak;
                                    out_buf.release();
                                    if (ec) {
                                        if (SEND_RETRY_TIMES > 0) {
                                            resend_if_do_send_failed(std::move(m_p), std::move(endpoint), std::move(handler));
                                        } else {
                                            std::cerr << fmt::format("Failed to send with error {}", ec.message());
                                            // Won't resend But still won't failed
                                        }
                                        // Try to resend
                                    } else {
                                        return handler(std::move(m_p), std::move(endpoint), {ec, length});
                                    }
                                 });
    }

    void do_receive(std::unique_ptr<Message> m_p, Handler handler)
    {
        std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint = std::make_unique<boost::asio::ip::udp::endpoint>();
        std::unique_ptr<MessageBuffer> in_buf = std::make_unique<MessageBuffer>();
        socket_.async_receive_from(boost::asio::buffer(in_buf->buffer), *endpoint,
                                [this, m_p = std::move(m_p), endpoint = std::move(endpoint), handler = std::move(handler), in_buf = std::move(in_buf)]
                                (boost::system::error_code ec, std::size_t length) mutable -> void {
                                    assert(length == Message::size());
                                    // We should first deserialize and avoid recursive memory leakage
                                    m_p->deserialize_from(in_buf->buffer);
                                    in_buf.release();
                                    if (ec) {
                                        // If we failed to receive, we just need to receive the next datagram
                                        do_receive(std::move(m_p), std::move(handler));
                                    }
                                    else {
                                        // Inform caller that data has been received ok.
                                        handler(std::move(m_p), std::move(endpoint), {ec, length});
                                    }
        });
    }
    boost::asio::ip::udp::socket& socket_;
};


#endif //PAXOS_NETWORK_H
