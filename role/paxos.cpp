//
// Created by Jiacheng Wu on 11/7/22.
//

#include "paxos.h"

void PaxosExecutor::dispatch_received_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras) {
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


void PaxosExecutor::dispatch_paxos_message(std::unique_ptr<Message> m_p,
                                           std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint,
                                           asio_handler_paras paras) {
    assert(Config::server_id_to_map[m_p->from_id] == *endpoint);
    uint32_t instance_seq = m_p->sequence;
    Instance* instance = instances.get_instance(instance_seq);
    switch (m_p->type) {
        case MessageType::PREPARE: {
            /** Acceptor Recv PREPARE from Proposer in Phase 1 **/
            // In dispatch_server_message, Proposer will obtain an instance and Send PREPARE to all
            std::unique_ptr<Message> promise_or_denial = instance->acceptor.on_prepare(std::move(m_p));
            instance->acceptor.promise_or_denial(std::move(promise_or_denial));
            break;
        }
        case MessageType::PROMISE: {
            /** Proposer Recv PROMISE from Acceptor in Phase 1 **/
            std::unique_ptr<Message> accept = instance->proposer.on_promise(std::move(m_p));

            break;
        }
        case MessageType::DENIAL: {
            /** Proposer Recv DENIAL (NOT PROMISE) from Acceptor in Phase 1 **/

            break;
        }
        case MessageType::ACCEPT: {
            /** Acceptor Recv ACCEPT from Propose in Phase 2 **/
            std::unique_ptr<Message> accepted_or_rejected = instance->acceptor.on_accept(std::move(m_p));
            instance->acceptor.accepted_or_reject(std::move(accepted_or_rejected));
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

void PaxosExecutor::dispatch_server_message(std::unique_ptr<Message> m_p,
                                            std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint,
                                            asio_handler_paras paras) {
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