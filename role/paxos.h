//
// Created by Jiacheng Wu on 11/7/22.
//

#ifndef PAXOS_PAXOS_H
#define PAXOS_PAXOS_H

#include "network.h"
#include "instance.h"
#include "fmt/core.h"
#include <iostream>
class PaxosExecutor {
  public:
    PaxosExecutor(boost::asio::io_context& io_context, std::uint16_t port, std::uint32_t id):
            id(id), instances(this),
            socket(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)),
            connect(socket) {

        this->number_of_nodes = 10;
    }

    ~PaxosExecutor() = default;

    std::uint32_t get_id() const {return id;}
    std::uint32_t get_number_of_nodes() const {return number_of_nodes;}

    Handler handler_wrapper(void (PaxosExecutor::*p)(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras)) {
        return std::bind_front(p, this);
    }


    void start() {
        connect.do_receive(std::make_unique<Message>(), handler_wrapper(&PaxosExecutor::dispatch_received_message));
    }


    void dispatch_received_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras);

    void dispatch_paxos_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras);

    void dispatch_server_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras);

    connection& get_connect() {
        return connect;
    }

  private:
    std::uint32_t id;
    std::uint32_t number_of_nodes;
    Instances instances;
    boost::asio::ip::udp::socket socket;
    connection connect;
    // connect should behave after socket for initialization orders
};


#endif //PAXOS_PAXOS_H
