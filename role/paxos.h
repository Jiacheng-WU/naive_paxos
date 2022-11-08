//
// Created by Jiacheng Wu on 11/7/22.
//

#ifndef PAXOS_PAXOS_H
#define PAXOS_PAXOS_H

#include "network.h"

class PaxosExecutor {
  public:
    PaxosExecutor(boost::asio::io_context& io_context, std::uint16_t port, std::uint32_t id):
            id(id),
            socket(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)),
            connect(socket) {}

    ~PaxosExecutor() = default;


    uint32_t get_id() {return id;}

    Handler handler_wrapper(void (PaxosExecutor::*p)(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras)) {
        return std::bind_front(p, this);
    }


    void start() {
        connect.do_receive(std::make_unique<Message>(), handler_wrapper(&PaxosExecutor::dispatch_received_message));
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
        // Do Next Wait Receive
        // We may need to detect some terminal condition here
        // connect.do_receive(std::make_unique<Message>(), handler_wrapper(&PaxosExecutor::dispatch_received_message));
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

    uint32_t id;
    boost::asio::ip::udp::socket socket;
    connection connect; // connect should behave after socket for initialization orders
};


#endif //PAXOS_PAXOS_H
