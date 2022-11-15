//
// Created by Jiacheng Wu on 10/8/22.
//

#include "network.hpp"


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

std::uint32_t get_random_number(std::uint32_t begin, std::uint32_t end) {
    std::random_device rd;
    std::uniform_int_distribution<std::uint32_t> ud(begin,end);
    std::mt19937 mt(rd());
    return ud(mt);
}

void Connection::do_send(std::unique_ptr<Message> m_p,
                         std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint,
                         Handler handler)
{
    std::unique_ptr<MessageBuffer> out_buf = std::make_unique<MessageBuffer>();
    m_p->serialize_to(out_buf->buffer);
    std::shared_ptr<boost::asio::ip::udp::endpoint> shared_endpoint =
            std::make_shared<boost::asio::ip::udp::endpoint>(*endpoint.release());
    socket_.async_send_to(boost::asio::buffer(out_buf->buffer), *shared_endpoint,
                          [this, m_p = std::move(m_p), shared_endpoint = shared_endpoint, handler = std::move(handler), out_buf = std::move(out_buf)]
                                  (boost::system::error_code ec, std::size_t length) mutable -> void {
                              // We should first release our buffer to avoid recursive memory leak;
                              std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint =
                                      std::make_unique<boost::asio::ip::udp::endpoint>(*shared_endpoint);
                              out_buf.release();
                              if (ec) {
                                  assert(length == Message::size());
                                  if (SEND_RETRY_TIMES > 0) {
                                      resend_if_do_send_failed(std::move(m_p), std::move(endpoint), std::move(handler));
                                  } else {
                                      BOOST_LOG_TRIVIAL(debug) << fmt::format("Failed to send {}:{} with error {}\n",
                                                                              endpoint->address().to_string(),
                                                                              endpoint->port(),
                                                                              ec.message());
                                      return handler(std::move(m_p), std::move(endpoint), {ec, length});
                                      // Won't resend But still won't failed
                                  }
                                  // Try to resend
                              } else {
                                  return handler(std::move(m_p), std::move(endpoint), {ec, length});
                              }
                          });
}

void Connection::do_receive(std::unique_ptr<Message> m_p, Handler handler) {
    std::shared_ptr<boost::asio::ip::udp::endpoint> shared_endpoint =
            std::make_shared<boost::asio::ip::udp::endpoint>();
    std::unique_ptr<MessageBuffer> in_buf = std::make_unique<MessageBuffer>();
    socket_.async_receive_from(boost::asio::buffer(in_buf->buffer), *shared_endpoint,
                               [this, m_p = std::move(m_p), shared_endpoint = std::move(shared_endpoint), handler = std::move(handler), in_buf = std::move(in_buf)]
                                       (boost::system::error_code ec, std::size_t length) mutable -> void {
                                   assert(length == Message::size());
                                   // We should first deserialize and avoid recursive memory leakage
                                   std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint =
                                           std::make_unique<boost::asio::ip::udp::endpoint>(*shared_endpoint);
                                   m_p->deserialize_from(in_buf->buffer);
                                   in_buf.release();
                                   if (ec) {
                                       BOOST_LOG_TRIVIAL(debug) << fmt::format("Failed to receive {}:{} with error {}\n",
                                                                               endpoint->address().to_string(),
                                                                               endpoint->port(),
                                                                               ec.message());
                                       do_receive(std::move(m_p), std::move(handler));
                                   }
                                   else {
                                       // Inform caller that data has been received ok.
                                       handler(std::move(m_p), std::move(endpoint), {ec, length});
                                   }
                               });
}