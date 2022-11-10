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

    void resend_if_send_to_failed(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras) {
        struct inner_retry {
            std::uint32_t times;
            connection& connect;
            inner_retry(std::uint32_t times, connection& connect): times(times), connect(connect) {};
            void operator()(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras) {
                if (this->times > 0) {
                    connect.do_send(std::move(m_p), std::move(endpoint), inner_retry(this->times - 1, this->connect));
                } else {
                    // Handle errors
                    std::cerr << fmt::format("Failed to send with error {}", paras.ec.message());
                    // Won't resend But still won't failed
                }
            }
        };
        std::uint32_t retry_times = 1;
        connect.do_send(std::move(m_p), std::move(endpoint), inner_retry(retry_times - 1, connect));
    }

    void dispatch_received_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras) {
        switch (m_p->type) {
            case MessageType::UNDEFINED:
                assert("The Message is not correctly Init");
                break;
            case MessageType::PREPARE:
            case MessageType::PROMISE:
            case MessageType::ACCEPT:
            case MessageType::DENIAL:
            case MessageType::ACCEPTED:
            case MessageType::LEARN:
                // For Paxos Protocal
                dispatch_paxos_message(std::move(m_p), std::move(endpoint), paras);
                break;
            case MessageType::SUBMIT:
            case MessageType::RESPONSE:
                dispatch_server_message(std::move(m_p), std::move(endpoint), paras);
                break;
            default:
                assert("Cannot Reach Here");
        }
        // TODO: Do Next Wait Receive
        // We may need to detect some terminal condition here
        connect.do_receive(std::make_unique<Message>(), handler_wrapper(&PaxosExecutor::dispatch_received_message));
    }

    void dispatch_paxos_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras) {
        uint32_t instance_seq = m_p->sequence;
        Instance* instance = instances.get_instance(instance_seq);
        switch (m_p->type) {
            case MessageType::PREPARE: {
                /** Acceptor Recv PREPARE from Proposer in Phase 1 **/
                // In dispatch_server_message, Proposer will obtain an instance and Send PREPARE to all
                std::unique_ptr<Message> promise_or_denial = instance->acceptor.on_prepare(std::move(m_p));
                promise_or_denial->from_id = this->get_id();
                connect.do_send(std::move(promise_or_denial), std::move(endpoint), handler_wrapper(&PaxosExecutor::resend_if_send_to_failed));
                break;
            }
            case MessageType::PROMISE: {
                /** Proposer Recv PROMISE from Acceptor in Phase 1 **/

                break;
            }
            case MessageType::DENIAL: {
                /** Proposer Recv DENIAL (NOT PROMISE) from Acceptor in Phase 1 **/

                break;
            }
            case MessageType::ACCEPT: {
                /** Acceptor Recv ACCEPT from Propose in Phase 2 **/
                std::unique_ptr<Message> accepted_or_rejected = instance->acceptor.on_prepare(std::move(m_p));
                accepted_or_rejected->from_id = this->get_id();
                connect.do_send(std::move(accepted_or_rejected), std::move(endpoint), handler_wrapper(&PaxosExecutor::resend_if_send_to_failed));
                break;
            }
            case MessageType::ACCEPTED: {
                /** Proposer (also Distinguished Leader) Recv ACCEPTED from Acceptor **/
                break;
            }
            case MessageType::REJECTED: {
                /** Proposer Recv REJECTED from Acceptor **/
                /** Could simply ignore it **/
                break;
            }
            case MessageType::INFORM: {
                /** Other Learners Recv INFORM from Distinguished Learner **/
                break;
            }
            case MessageType::LEARN: {
                /** Distinguish Learner Recv LEARN from Other Learners **/
                break;
            }
            default:
                assert("Cannot Reach Here in Dispatch Requests");
        }
    }

    void dispatch_server_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras) {
        uint32_t instance_seq = m_p->sequence;
        Instance* instance = instances.get_instance(instance_seq);
        switch (m_p->type) {
            case MessageType::SUBMIT:
                break;
            case MessageType::REDIRECT:
                break;
            default:
                assert("Connot Reach Here in Dispatch Server");
        }
    }

    uint32_t id;
    Instances instances;
    boost::asio::ip::udp::socket socket;
    connection connect; // connect should behave after socket for initialization orders
};


#endif //PAXOS_PAXOS_H
