//
// Created by Jiacheng Wu on 11/7/22.
//

#ifndef PAXOS_SERVER_HPP
#define PAXOS_SERVER_HPP

#include <iostream>
#include <unordered_map>
#include <queue>
#include <set>
#include <format>

#include "config.hpp"
#include "logger.hpp"
#include "network.hpp"
#include "instance.hpp"


class PaxosServer {
  public:
    // We would like to load config outside
    PaxosServer(boost::asio::io_context &io_context, std::uint32_t id, std::unique_ptr<Config> config) :
            instances(this),
            socket(io_context,
                   boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(),
                                                  config->get_addr_by_id(id)->port())),
            connect(std::make_unique<Connection>(socket)),
//            random_resubmit_timer(socket.get_executor()),
            logger(std::make_unique<Logger>(config->get_acceptor_file_path(id), config->get_learner_file_path(id),
                                            this)) {
        this->id = id;
//        this->leader_id = 0;
        // this->submit_cmd_seq = 0;
        this->executed_cmd_seq = 0;
        this->config = std::move(config);
        this->number_of_nodes = this->config->number_of_nodes;
        // Since we do not want connect observe the Config thus just set here.
        this->connect->set_send_retry_time(this->config->network_send_retry_times);

        if (this->config->need_recovery) {
            recover();
        }
    }

    ~PaxosServer() = default;

    std::uint32_t get_id() const { return id; }

    std::uint32_t get_number_of_nodes() const { return number_of_nodes; }

    std::uint32_t get_executed_cmd_seq() const {return executed_cmd_seq;}

    Handler handler_wrapper(void (PaxosServer::*p)(std::unique_ptr<Message> m_p,
                                                   std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint,
                                                   asio_handler_paras paras)) {
        return std::bind_front(p, this);
    }


    void start() {
        connect->do_receive(std::make_unique<Message>(), handler_wrapper(&PaxosServer::dispatch_received_message));

    }


    void recover();

    void stop() {
        socket.close();
    }

//    void heartbeat() {
//
//        if (get_id() != leader_id) {
//            return;
//        }
//
//        std::unique_ptr<Message> heartbeat = std::make_unique<Message>();
//        heartbeat->type = MessageType::HEARTBEAT;
//        heartbeat->from_id = this->get_id();
//
//        for(std::uint32_t node_id = 0; node_id < this->get_number_of_nodes(); node_id++) {
//            // We need to clone the unique_ptr<Message> and just send to all nodes;
//            std::unique_ptr<Message> prepare_copy = heartbeat->clone();
//            std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint = this->config->get_addr_by_id(node_id);
//            this->connect->do_send(std::move(prepare_copy), std::move(endpoint), do_nothing_handler);
//        }
//    }

//    void start_leader_heartbeat() {
//        heartbeat();
//        // random_resubmit_timer.cancel();
//        random_resubmit_timer.expires_after(std::chrono::seconds(config->send_heartbeat_interval_seconds));
//        random_resubmit_timer.async_wait([this](const boost::system::error_code& error) {
//            if (error) { return; }
//            this->start_leader_heartbeat();
//        });
//    }

//    void stop_leader_heartbeat() {
//        random_resubmit_timer.cancel();
//    }

//    void reset_nonleader_heartbeat() {
//        nonleader_heartbeat_timer.cancel();
//        nonleader_heartbeat_timer.expires_after(std::chrono::seconds(config->ack_heartbeat_interval_seconds));
//        nonleader_heartbeat_timer.async_wait([this](const boost::system::error_code& error){
//            if (error) {
//                // cancel operation
//                return;
//            }
//            if (get_id() == leader_id) {
//                return;
//            }
//            // Process a new leader election !!
//            // Obtain a new Instance id;
//            // May request the learner until got a instance of leader
//        });
//    }

//    void on_heartbeat(std::unique_ptr<Message> heartbeat) {
//        {
//            if (get_id() == leader_id || heartbeat->from_id != leader_id) {
//                return;
//            }
//        }
//        reset_nonleader_heartbeat();
//    }

    void
    dispatch_received_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint,
                              asio_handler_paras paras);

    void dispatch_paxos_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint,
                                asio_handler_paras paras);

    void dispatch_server_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint,
                                 asio_handler_paras paras);

//    Connection& get_connect() {
//        return connect;
//    }



    std::vector<std::unique_ptr<Message>> try_execute_commands(std::unique_ptr<Message> command);

    std::unique_ptr<Message> on_submit_of_server(std::unique_ptr<Message> submit);

    boost::asio::ip::udp::socket socket;
    std::unique_ptr<Connection> connect;
    std::unique_ptr<Config> config;
//    boost::asio::steady_timer random_resubmit_timer;
//    boost::asio::steady_timer nonleader_heartbeat_timer; // For check
    std::unique_ptr<Logger> logger;
    Instances instances;
  private:

    std::unique_ptr<Message> execute_command(std::unique_ptr<Message> command);

    std::unique_ptr<Message> response(std::unique_ptr<Message> response);

    std::uint32_t id;
    std::uint32_t number_of_nodes;


//    std::uint32_t get_instance_sequence() {
//        submit_cmd_seq++;
//        return submit_cmd_seq;
//    }
    // State for both elect leader and execute command from Instance
    mutable std::mutex server_state_mutex;
//    std::uint32_t leader_id;
    // std::uint32_t submit_cmd_seq;

    using object_id_t = std::uint32_t;
    using client_id_t = std::uint64_t;
    using client_once_t = std::uint32_t;
    using lock_client_id_t = client_id_t;

    std::unordered_map<std::uint32_t, ProposalValue> seq_to_expected_values;

    std::unordered_map<object_id_t, lock_client_id_t> object_lock_state;

    std::uint32_t executed_cmd_seq;



    class Message_Command_Less {
      public:
        bool operator()(const std::unique_ptr<Message> &__x, const std::unique_ptr<Message> &__y) const {
            return __x->sequence < __y->sequence;
        }
    };

    std::set<std::unique_ptr<Message>, Message_Command_Less> cmd_min_set;


    struct hash_pair {
        template<class T1, class T2>
        size_t operator()(const std::pair<T1, T2> &p) const {
            auto hash1 = std::hash<T1>{}(p.first);
            auto hash2 = std::hash<T2>{}(p.second);

            if (hash1 != hash2) {
                return hash1 ^ hash2;
            }
            // If hash1 == hash2, their XOR is zero.
            return hash1;
        }
    };

    std::unordered_map<std::pair<client_id_t, client_once_t>, std::unique_ptr<Message>, hash_pair> client_ops_to_response;

    // connect should behave after socket for initialization orders
};


#endif //PAXOS_SERVER_HPP
