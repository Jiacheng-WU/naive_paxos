//
// Created by Jiacheng Wu on 11/7/22.
//

#ifndef PAXOS_PAXOS_H
#define PAXOS_PAXOS_H

#include "network.h"
#include "instance.h"
#include "fmt/core.h"
#include "config.h"
#include <iostream>
#include <unordered_map>
#include <queue>
#include <set>
class PaxosServer {
  public:
    // We would like to load config outside
    PaxosServer(boost::asio::io_context& io_context, std::unique_ptr<Config> config):
            instances(this),
            socket(io_context,
                   boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(),
                                                              config->get_addr_by_id(config->get_current_id())->port())),
            connect(std::make_unique<Connection>(socket)) {
        this->id = config-> get_current_id();
        this->number_of_nodes = config->get_number_of_nodes();
        this->submit_cmd_seq = 0;
        this->executed_cmd_seq = 0;
    }

    ~PaxosServer() = default;

    std::uint32_t get_id() const {return id;}
    std::uint32_t get_number_of_nodes() const {return number_of_nodes;}


    Handler handler_wrapper(void (PaxosServer::*p)(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras)) {
        return std::bind_front(p, this);
    }


    void start() {
        connect->do_receive(std::make_unique<Message>(), handler_wrapper(&PaxosServer::dispatch_received_message));
    }


    void dispatch_received_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras);

    void dispatch_paxos_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras);

    void dispatch_server_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras);

//    Connection& get_connect() {
//        return connect;
//    }



    void try_execute_command_and_response(std::unique_ptr<Message> command);

    std::unique_ptr<Message> on_submit_of_server(std::unique_ptr<Message> submit);

    std::unique_ptr<Connection> connect;
    std::unique_ptr<Config> config;
  private:

    std::unique_ptr<Message> execute_command(std::unique_ptr<Message> command);
    void response(std::unique_ptr<Message> response);

    std::uint32_t id;
    std::uint32_t number_of_nodes;
    Instances instances;
    boost::asio::ip::udp::socket socket;


//    std::uint32_t get_instance_sequence() {
//        submit_cmd_seq++;
//        return submit_cmd_seq;
//    }
    // State for both elect leader and execute command from Instance
    mutable std::mutex server_state_mutex;
    std::uint32_t leader_id;
    std::uint32_t submit_cmd_seq;

    using object_id_t = std::uint32_t;
    using lock_client_id_t = std::uint64_t;

    std::unordered_map<std::uint32_t, std::unique_ptr<boost::asio::ip::udp::endpoint>> seq_to_clients;

    std::unordered_map<object_id_t, lock_client_id_t> object_lock_state;

    std::uint32_t executed_cmd_seq;

    class Message_Command_Less {
      public:
        bool operator()(const std::unique_ptr<Message>& __x, const std::unique_ptr<Message>& __y) const
        {return __x->sequence < __y->sequence;}
    };

    std::set<std::unique_ptr<Message>, Message_Command_Less> cmd_min_set;
    // connect should behave after socket for initialization orders
};


#endif //PAXOS_PAXOS_H
