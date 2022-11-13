//
// Created by Jiacheng Wu on 11/7/22.
//

#include "server.h"

void PaxosServer::dispatch_received_message(std::unique_ptr<Message> m_p, std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint, asio_handler_paras paras) {
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
        case MessageType::UNDECIDE:
            // For Paxos Protocal
            dispatch_paxos_message(std::move(m_p), std::move(endpoint), paras);
            break;
        case MessageType::SUBMIT:
        case MessageType::HEARTBEAT:
            dispatch_server_message(std::move(m_p), std::move(endpoint), paras);
            break;
        default:
            assert("Cannot Reach Here");
    }
    // TODO: Do Next Wait Receive
    // We may need to detect some terminal condition here
    connect->do_receive(std::make_unique<Message>(), handler_wrapper(&PaxosServer::dispatch_received_message));
}


void PaxosServer::dispatch_paxos_message(std::unique_ptr<Message> m_p,
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
            if (accept != nullptr) {
                instance->proposer.accept(std::move(accept));
            }
            // We need timeout mechanisms
            break;
        }
        case MessageType::DENIAL: {
            /** Proposer Recv DENIAL (NOT PROMISE) from Acceptor in Phase 1 **/
            std::unique_ptr<Message> resubmit = instance->proposer.on_denial(std::move(m_p));
            if (resubmit != nullptr) {
                // If we received majority of request, the we need to resubmit the proposal and prepare again
                std::unique_ptr<Message> prepare = instance->proposer.on_submit(std::move(resubmit));
                instance->proposer.prepare(std::move(prepare));
            }
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
            std::unique_ptr<Message> inform = instance->learner.on_accepted(std::move(m_p));
            if (inform != nullptr) {
                // Over a majority of acceptor achieve consensus
                instance->learner.inform(std::move(inform));
            }
            break;
        }
        case MessageType::REJECTED: {
            /** Proposer Recv REJECTED from Acceptor **/
            /** Could simply ignore it **/
            break;
        }
        case MessageType::INFORM: {
            /** Other Learners Recv INFORM from Distinguished Learner **/
            std::unique_ptr<Message> command = instance->learner.on_inform(std::move(m_p));
            this->try_execute_command_and_response(std::move(command));
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

void PaxosServer::dispatch_server_message(std::unique_ptr<Message> m_p,
                                          std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint,
                                          asio_handler_paras paras) {
//    uint32_t instance_seq = m_p->sequence;
//    Instance* instance = instances.get_instance(instance_seq);
    switch (m_p->type) {
        case MessageType::SUBMIT: {
            std::unique_ptr<Message> submit_or_redirect = this->on_submit_of_server(std::move(m_p));
            if (submit_or_redirect->type == MessageType::REDIRECT) {
                std::unique_ptr<Message> redirect = std::move(submit_or_redirect);
                connect->do_send(std::move(redirect), std::move(endpoint), do_nothing_handler);
            } else {
                std::unique_ptr<Message> submit = std::move(submit_or_redirect);
                uint32_t instance_seq = submit->sequence;
                Instance* instance = instances.get_instance(instance_seq);
                seq_to_clients.emplace(instance_seq, std::move(endpoint));
                std::unique_ptr<Message> prepare = instance->proposer.on_submit(std::move(submit));
                instance->proposer.prepare(std::move(prepare));
            }
            break;
        }
        case MessageType::HEARTBEAT: {
            this->on_heartbeat(std::move(m_p));
        }
        default:
            assert("Connot Reach Here in Dispatch Server");
    }
}

std::unique_ptr<Message> PaxosServer::on_submit_of_server(std::unique_ptr<Message> submit_from_client) {
    std::lock_guard<std::mutex> lock(server_state_mutex);
    if (leader_id != id) {
        // We need to redirect meassage
        std::unique_ptr<Message> redirect = std::move(submit_from_client);
        redirect->type = MessageType::REDIRECT;
        redirect->leader_id = this->leader_id;
        return std::move(redirect);
    } else {
        submit_cmd_seq++;
        std::size_t sequence = submit_cmd_seq;
        std::unique_ptr<Message> submit = std::move(submit_from_client);
        submit->sequence = sequence;
        submit->type = MessageType::SUBMIT;
        return std::move(submit);
    }

}

static uint64_t get_uint64_from_udp_ipv4_endpoint(std::unique_ptr<boost::asio::ip::udp::endpoint>& endpoint) {
    uint32_t ip = endpoint->address().to_v4().to_uint();
    uint32_t port = endpoint->port();
    uint64_t result = uint64_t(ip) << 32 | port;
    return result;
}

static uint64_t unused_udp_ipv4_number = ~0ull;

std::unique_ptr<Message> PaxosServer::execute_command(std::unique_ptr<Message> command) {
    ProposalValue cmd = command->proposal.value;
    switch (cmd.operation) {
        case ProposalValue::UNDEFINED: {
            break;
        }
        case ProposalValue::ELECT_LEADER: {
            if (leader_id != cmd.object) {
                if (leader_id == id) {
                    stop_leader_heartbeat();
                }
                leader_id = cmd.object;
                if (leader_id == id) {
                    start_leader_heartbeat();
                    nonleader_heartbeat_timer.cancel();
                } else {
                    reset_nonleader_heartbeat();
                }
            }
            break;
        }
        case ProposalValue::LOCK: {
            std::unique_ptr<Message> response = std::move(command);
            response->type = MessageType::RESPONSE;
            object_id_t object_id = cmd.object;
            if (!object_lock_state.contains(object_id)) {
                object_lock_state.insert({object_id, unused_udp_ipv4_number});
            }
            lock_client_id_t current_client_id = get_uint64_from_udp_ipv4_endpoint(seq_to_clients[response->sequence]);
            lock_client_id_t locked_client_id = object_lock_state[object_id];
            if (locked_client_id == unused_udp_ipv4_number) {
                object_lock_state[object_id] = current_client_id;
                response->proposal.value.operation = ProposalValue::LOCK_SUCCEED;
            } else if (locked_client_id == current_client_id) {
                response->proposal.value.operation = ProposalValue::LOCK_AGAIN;
            } else {
                response->proposal.value.operation = ProposalValue::LOCK_FAILED;
            }
            return std::move(response);
            break;
        }
        case ProposalValue::UNLOCK: {
            std::unique_ptr<Message> response = std::move(command);
            response->type = MessageType::RESPONSE;
            object_id_t object_id = cmd.object;
            if (!object_lock_state.contains(object_id)) {
                object_lock_state.insert({object_id, unused_udp_ipv4_number});
            }
            lock_client_id_t current_client_id = get_uint64_from_udp_ipv4_endpoint(seq_to_clients[response->sequence]);
            lock_client_id_t locked_client_id = object_lock_state[object_id];
            if (locked_client_id == unused_udp_ipv4_number) {
                response->proposal.value.operation = ProposalValue::UNLOCK_AGAIN;
            } else if (locked_client_id == current_client_id) {
                response->proposal.value.operation = ProposalValue::UNLOCK_SUCCEED;
                object_lock_state[object_id] = unused_udp_ipv4_number;
            } else {
                response->proposal.value.operation = ProposalValue::UNLOCK_FAILED;
            }
            return std::move(response);
            break;
        }
        case ProposalValue::NOOPS: {
            break;
        }
        default: {
            assert("Cannot Reach Here");
        }
    }
    return nullptr;
}

void PaxosServer::response(std::unique_ptr<Message> response) {
    std::lock_guard<std::mutex> lock(server_state_mutex);
    std::unique_ptr<boost::asio::ip::udp::endpoint> client_endpoints = std::move(seq_to_clients.at(response->sequence));
    seq_to_clients.erase(response->sequence);
    connect->do_send(std::move(response), std::move(client_endpoints), do_nothing_handler);
}

void PaxosServer::try_execute_command_and_response(std::unique_ptr<Message> command) {
    // TODO: submit command to a minheap. and execute consecutive command if a hole is filled
    std::unique_lock<std::mutex> lock(server_state_mutex);

    if (command->sequence == executed_cmd_seq + 1) {
        std::unique_ptr<Message> response = execute_command(std::move(command));
        executed_cmd_seq++;
        lock.unlock();
        if (response != nullptr) {
            this->response(std::move(response));
        }

        // Try execute the following command until we read a hole
        while(true) {
            lock.lock();
            std::size_t current_wait_sequence = (*cmd_min_set.begin())->sequence;
            if (current_wait_sequence == executed_cmd_seq + 1) {
                std::unique_ptr<Message> next_command = std::move(cmd_min_set.extract(cmd_min_set.begin()).value());
                std::unique_ptr<Message> response = execute_command(std::move(command));
                executed_cmd_seq++;
                lock.unlock();
                if(response != nullptr) {
                    this->response(std::move(response));
                }
            } else {
                lock.unlock();
                break;
            }
        }
    } else if (command->sequence <= executed_cmd_seq) {
        // Ignore this case
        lock.unlock();
    } else {
        // Here is a hole
        cmd_min_set.emplace(std::move(command));
        lock.unlock();
    }
}

