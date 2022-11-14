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
        case MessageType::INFORM:
        case MessageType::LEARN:
        case MessageType::UNDECIDE:
            // For Paxos Protocal
            dispatch_paxos_message(std::move(m_p), std::move(endpoint), paras);
            break;
        case MessageType::SUBMIT:
//        case MessageType::HEARTBEAT:
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
    // assert(Config::server_id_to_map[m_p->from_id] == *endpoint);
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
            fmt::print("After Send Accept\n");
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
                fmt::print("After inform\n");
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
            std::vector<std::unique_ptr<Message>> executed_commands = this->try_execute_commands(std::move(command));
            if (!executed_commands.empty()) {
                for(auto&& cmd: executed_commands) {
                    if (cmd->from_id == id) {
                        fmt::print("Before RESPONSE\n");
                        std::unique_ptr<Message> command = std::move(cmd);
                        std::unique_ptr<Message> resubmit = this->response(std::move(command));
                        if (resubmit != nullptr) {
                            // Add random delay for submit again
                            std::shared_ptr<boost::asio::steady_timer> random_resubmit_timer =
                                    std::make_shared<boost::asio::steady_timer>(socket.get_executor());
                            random_resubmit_timer->expires_after(std::chrono::milliseconds(get_random_number(0, 1000)));
                            random_resubmit_timer->async_wait([this, resubmit = std::move(resubmit), random_resubmit_timer]
                            (const boost::system::error_code& error) mutable{
                                if (error) {/* DO NOT RETURN, still send even it will be cancelled */}
                                std::unique_ptr<Message> submit = this->on_submit_of_server(std::move(resubmit));
                                // std::unique_ptr<Message> submit = std::move(submit_or_redirect);
                                // We cannot modify the client_id since it is still the original one
                                uint32_t instance_seq = submit->sequence;
                                Instance *instance = instances.get_instance(instance_seq);
                                // We need to reemplace with a newer sequence number;
                                seq_to_expected_values.emplace(instance_seq, submit->proposal.value);
                                std::unique_ptr<Message> prepare = instance->proposer.on_submit(std::move(submit));
                                instance->proposer.prepare(std::move(prepare));
                            });
                        }
                    }
                }
            }
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

            std::unique_ptr<Message> submit = this->on_submit_of_server(std::move(m_p));
            // We need to set client_id and maintain seq with proposal relation
            submit->proposal.value.client_id = get_uint64_from_udp_ipv4_endpoint(endpoint);
            uint32_t instance_seq = submit->sequence;
            Instance* instance = instances.get_instance(instance_seq);
            seq_to_expected_values.emplace(instance_seq, submit->proposal.value);
            std::unique_ptr<Message> prepare = instance->proposer.on_submit(std::move(submit));

            if (prepare != nullptr) {
                fmt::print("SUBMIT server {} recv client {}\n", id, prepare->proposal.value.client_once);
                instance->proposer.prepare(std::move(prepare));
            }
            fmt::print("Finish PREPARE\n");
            break;
        }
//        case MessageType::HEARTBEAT: {
//            this->on_heartbeat(std::move(m_p));
//        }
        default:
            assert("Connot Reach Here in Dispatch Server");
    }
}

std::unique_ptr<Message> PaxosServer::on_submit_of_server(std::unique_ptr<Message> submit_from_client) {
    std::lock_guard<std::mutex> lock(server_state_mutex);
//    if (leader_id != id) {
//        // We need to redirect meassage
//        std::unique_ptr<Message> redirect = std::move(submit_from_client);
//        redirect->type = MessageType::REDIRECT;
//        redirect->leader_id = this->leader_id;
//        return std::move(redirect);
//    } else {
    std::size_t sequence = executed_cmd_seq + 1;
    std::unique_ptr<Message> submit = std::move(submit_from_client);
    submit->sequence = sequence;
    submit->type = MessageType::SUBMIT;
    return std::move(submit);
//    }

}



std::unique_ptr<Message> PaxosServer::execute_command(std::unique_ptr<Message> command) {
    ProposalValue cmd = command->proposal.value;
    switch (cmd.operation) {
        case ProposalValue::UNDEFINED: {
            break;
        }
//        case ProposalValue::ELECT_LEADER: {
//            if (leader_id != cmd.object) {
//                if (leader_id == id) {
//                    stop_leader_heartbeat();
//                }
//                leader_id = cmd.object;
//                if (leader_id == id) {
//                    start_leader_heartbeat();
//                    nonleader_heartbeat_timer.cancel();
//                } else {
//                    reset_nonleader_heartbeat();
//                }
//            }
//            break;
//        }
        case ProposalValue::LOCK: {
            std::unique_ptr<Message> response = std::move(command);
            response->type = MessageType::RESPONSE;
            object_id_t object_id = cmd.object;
            if (!object_lock_state.contains(object_id)) {
                object_lock_state.insert({object_id, unused_udp_ipv4_number});
            }
            lock_client_id_t current_client_id = response->proposal.value.client_id;
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
            lock_client_id_t current_client_id = response->proposal.value.client_id;
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

std::unique_ptr<Message> PaxosServer::response(std::unique_ptr<Message> response) {
    std::lock_guard<std::mutex> lock(server_state_mutex);

    std::unique_ptr<Message> resubmit = nullptr;
    if (seq_to_expected_values.contains(response->sequence)) {
        ProposalValue expected_value = seq_to_expected_values[response->sequence];
        if (expected_value.client_id != response->proposal.value.client_id ||
            expected_value.client_once != response->proposal.value.client_once) {
            resubmit = response->clone();
            resubmit->proposal.value = expected_value;
        }
        seq_to_expected_values.erase(response->sequence);
    }

    std::unique_ptr<boost::asio::ip::udp::endpoint> client_endpoints = get_udp_ipv4_endpoint_from_uint64_t(response->proposal.value.client_id);
    fmt::print("Send to clients {} {}", client_endpoints->address().to_string(), resubmit == nullptr);
    connect->do_send(std::move(response), std::move(client_endpoints), do_nothing_handler);
    return resubmit;
}

// The response have already done by the main learned once it find it should reponse
std::vector<std::unique_ptr<Message>> PaxosServer::try_execute_commands(std::unique_ptr<Message> command) {
    // TODO: submit command to a minheap. and execute consecutive command if a hole is filled
    std::lock_guard<std::mutex> lock(server_state_mutex);

    std::vector<std::unique_ptr<Message>> executed_commands;
    if (command->sequence == executed_cmd_seq + 1) {
        fmt::print("Before Execute Command\n");
        std::unique_ptr<Message> response = execute_command(std::move(command));
        if (response != nullptr) {
            executed_commands.emplace_back(std::move(response));
        }
        executed_cmd_seq++;
//        if (response != nullptr) {
//            this->response(std::move(response));
//        }

        // Try execute the following command until we read a hole
        while(!cmd_min_set.empty()) {
            std::size_t current_wait_sequence = (*cmd_min_set.begin())->sequence;
            if (current_wait_sequence == executed_cmd_seq + 1) {
                std::unique_ptr<Message> next_command = std::move(cmd_min_set.extract(cmd_min_set.begin()).value());
                std::unique_ptr<Message> response = execute_command(std::move(command));
                executed_cmd_seq++;
                if (response != nullptr) {
                    executed_commands.emplace_back(std::move(response));
                }
            } else {
                break;
            }
        }
    } else if (command->sequence <= executed_cmd_seq) {
        // Ignore this case
    } else {
        // Here is a hole
        cmd_min_set.emplace(std::move(command));
    }
    return executed_commands;
}

void PaxosServer::recover()  {
    std::vector<std::uint32_t> hole_sequence;
    std::uint32_t max_sequence;
    bool need_recover = this->logger->recover_from_log(hole_sequence, max_sequence);
    if (!need_recover) {
        return;
    } else {
        // Propose no ops to hole_sequence and to learn server_id;
    }
}

