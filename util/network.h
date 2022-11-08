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
#include <message.h>
class connection {
  public:
    /// Constructor.
    connection(boost::asio::io_context& io_context, uint16_t port)
    : socket_(io_context,
              boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)) {

    }

    void start() {

    }

    /// Get the underlying socket. Used for making a connection or for accepting
    /// an incoming connection.
    boost::asio::ip::udp::socket& socket()
    {
        return socket_;
    }

    struct asio_handler_paras {

        boost::system::error_code ec;
        std::size_t length;
    };
    using Handler = std::function<void(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras)>;

    void do_send(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, Handler handler)
    {
        boost::iostreams::basic_array_sink<char> sr(out_message_buffer, sizeof(out_message_buffer));
        boost::iostreams::stream< boost::iostreams::basic_array_sink<char> > source(sr);
        boost::archive::binary_oarchive archive(source);
        archive << *m_p;
        socket_.async_send_to(boost::asio::buffer(out_message_buffer), *endpoint,
                                 [handler, m_p = std::move(m_p), endpoint = std::move(endpoint)]
                                 (boost::system::error_code ec, std::size_t length)  mutable
                                 { return handler(std::move(m_p), std::move(endpoint), {ec, length}); });
    }

    void do_receive(std::unique_ptr<Message> m_p, Handler handler)
    {
        std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint;
        socket_.async_receive_from(boost::asio::buffer(in_message_buffer), *endpoint,
                                [this, m_p = std::move(m_p), endpoint = std::move(endpoint), &handler] (boost::system::error_code ec, std::size_t length) mutable -> void {
                                    if (ec) { handler(std::move(m_p), std::move(endpoint), {ec, length});}
                                    else {
                                        try {
                                            std::string archive_data(in_message_buffer, sizeof(in_message_buffer));
                                            std::istringstream archive_stream(archive_data);
                                            boost::archive::binary_iarchive archive(archive_stream);
                                            archive >> *m_p;
                                        } catch (std::exception& e) {
                                            // Unable to decode data.
                                            boost::system::error_code error(boost::asio::error::invalid_argument);
                                            handler(std::move(m_p), std::move(endpoint), {error, length});
                                            return;
                                        }
                                        // Inform caller that data has been received ok.
                                        handler(std::move(m_p), std::move(endpoint), {ec, length});
                                    }
        });
    }

    void dispatch_received_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras) {
        switch (m_p->type) {
            case MessageType::PREPARE:
            case MessageType::PREPARE_REPLY:
            case MessageType::ACCEPT:
            case MessageType::ACCEPT_REPLY:
            case MessageType::LEARN:
            case MessageType::LEARN_REPLY:
                // For Paxos Protocal
                dispatch_paxos_message(std::move(m_p), std::move(endpoint), paras);
                break;
            case MessageType::SUBMIT:
            case MessageType::SUBMIT_REPLY:

                break;
            default:
                assert("Cannot Reach Here");
        }
    }

    void dispatch_paxos_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras) {
        Message& m = *m_p;
        uint32_t instance_seq = m.instance;

        switch (m_p->type) {
            case MessageType::PREPARE:
                break;
            case MessageType::PREPARE_REPLY:
                break;
            case MessageType::ACCEPT:
                break;
            case MessageType::ACCEPT_REPLY:
                break;
            case MessageType::LEARN:
                break;
            case MessageType::LEARN_REPLY:
                break;
            default:
                assert("Cannot Reach Here");
        }
    }


    char out_message_buffer[Message::size()];
    char in_message_buffer[Message::size()];
    boost::asio::ip::udp::socket socket_;

};


#endif //PAXOS_NETWORK_H
