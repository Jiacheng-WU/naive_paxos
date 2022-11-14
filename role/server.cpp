//
// Created by Jiacheng Wu on 11/7/22.
//

#include "server.hpp"

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
            /** Proposer Recv DENIAL (NOT PROMISE) from Acceptor in Phase 1 **/
            std::unique_ptr<Message> resubmit = instance->learner.on_rejected(std::move(m_p));
            if (resubmit != nullptr) {
                // If we received majority of request, the we need to resubmit the proposal and prepare again
                std::unique_ptr<boost::asio::ip::udp::endpoint> client_endpoint = get_udp_ipv4_endpoint_from_uint64_t(resubmit->proposal.value.client_id);
                BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Server {} Need Resubmit {} {} from Client {}:{} {} since Instance received majority Denial\n",
                                                        resubmit->sequence, this->get_id(),
                                                        magic_enum::enum_name(resubmit->proposal.value.operation),
                                                        resubmit->proposal.value.object,
                                                        client_endpoint->address().to_string(),
                                                        client_endpoint->port(),
                                                        resubmit->proposal.value.client_once);
                std::unique_ptr<Message> prepare = instance->proposer.on_submit(std::move(resubmit));
                instance->proposer.prepare(std::move(prepare));
            }
            break;
        }
        case MessageType::INFORM: {
            /** Other Learners Recv INFORM from Distinguished Learner **/
            std::unique_ptr<Message> command = instance->learner.on_inform(std::move(m_p));
            if (command == nullptr) {
                // if has informed before, we must have executed the command before
                break;
            }
            std::vector<std::unique_ptr<Message>> executed_commands = this->try_execute_commands(std::move(command));
            if (executed_commands.empty()) {
                break;
            }
            for (auto &&cmd: executed_commands) {
                if (cmd->from_id != id) {
                    // It represents that current server is not response to the client requests;
                    continue;
                }
                std::unique_ptr<Message> command = std::move(cmd);
                std::unique_ptr<Message> resubmit = this->response(std::move(command));
                if (resubmit == nullptr) {
                    // We have already finish the commands !!
                    continue;
                }
                // Add random delay for submit again
                std::unique_ptr<boost::asio::ip::udp::endpoint> client_endpoint = get_udp_ipv4_endpoint_from_uint64_t(resubmit->proposal.value.client_id);
                BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Server {} Need Resubmit {} {} from Client {}:{} {} since Instance didn't select this Commands\n",
                                                        resubmit->sequence, this->get_id(),
                                                        magic_enum::enum_name(resubmit->proposal.value.operation),
                                                        resubmit->proposal.value.object,
                                                        client_endpoint->address().to_string(),
                                                        client_endpoint->port(),
                                                        resubmit->proposal.value.client_once);
                std::shared_ptr<boost::asio::steady_timer> random_resubmit_timer =
                        std::make_shared<boost::asio::steady_timer>(socket.get_executor());
                random_resubmit_timer->expires_after(std::chrono::milliseconds(get_random_number(0, 1000)));
                random_resubmit_timer->async_wait(
                        [this, resubmit = std::move(resubmit), random_resubmit_timer]
                        (const boost::system::error_code &error) mutable {
                            if (error) {/* DO NOT RETURN, still send even it will be cancelled */}
                            std::unique_ptr<Message> submit = this->on_submit_of_server(std::move(resubmit));
                            // We cannot modify the client_id since it is still the original one
                            uint32_t instance_seq = submit->sequence;
                            Instance *instance = instances.get_instance(instance_seq);
                            // We need to reemplace with a newer sequence number;
                            seq_to_expected_values.emplace(instance_seq, submit->proposal.value);
                            std::unique_ptr<Message> prepare = instance->proposer.on_submit(std::move(submit));
                            instance->proposer.prepare(std::move(prepare));
                        });
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
            submit->proposal.value.client_id = get_uint64_from_udp_ipv4_endpoint(endpoint);

            BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Server {} Recv Submit {} {} from Client {}:{} {}\n",
                                                    submit->sequence, this->get_id(),
                                                    magic_enum::enum_name(submit->proposal.value.operation),
                                                    submit->proposal.value.object,
                                                    endpoint->address().to_string(),
                                                    endpoint->port(),
                                                    submit->proposal.value.client_once);
            // We need to set client_id and maintain seq with proposal relation

            uint32_t instance_seq = submit->sequence;
            Instance* instance = instances.get_instance(instance_seq);
            seq_to_expected_values.emplace(instance_seq, submit->proposal.value);
            std::unique_ptr<Message> prepare = instance->proposer.on_submit(std::move(submit));

            if (prepare != nullptr) {
                instance->proposer.prepare(std::move(prepare));
            }
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

    std::unique_ptr<boost::asio::ip::udp::endpoint> client_endpoint = get_udp_ipv4_endpoint_from_uint64_t(response->proposal.value.client_id);
    BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Server {} Async Send Response {} {} to Client {}:{} {}\n",
                                            response->sequence, this->get_id(),
                                            magic_enum::enum_name(response->proposal.value.operation),
                                            response->proposal.value.object,
                                            client_endpoint->address().to_string(),
                                            client_endpoint->port(),
                                            response->proposal.value.client_once);
    connect->do_send(std::move(response), std::move(client_endpoint), do_nothing_handler);
    return resubmit;
}

// The response have already done by the main learned once it find it should reponse
std::vector<std::unique_ptr<Message>> PaxosServer::try_execute_commands(std::unique_ptr<Message> command) {
    // TODO: submit command to a minheap. and execute consecutive command if a hole is filled
    std::lock_guard<std::mutex> lock(server_state_mutex);

    std::vector<std::unique_ptr<Message>> executed_commands;
    if (command->sequence == executed_cmd_seq + 1) {
        std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint = get_udp_ipv4_endpoint_from_uint64_t(command->proposal.value.client_id);
        BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Server {} Execute Command {} {} from Client {}:{} {}\n",
                   command->sequence, this->get_id(),
                   magic_enum::enum_name(command->proposal.value.operation),
                   command->proposal.value.object,
                   endpoint->address().to_string(),
                   endpoint->port(),
                   command->proposal.value.client_once);
        std::unique_ptr<Message> response = execute_command(std::move(command));
        if (response != nullptr) {
            BOOST_LOG_TRIVIAL(trace) << fmt::format("Inst Seq {} : Server {} Command Result {} {} to Client {}:{} {}\n",
                       response->sequence, this->get_id(),
                       magic_enum::enum_name(response->proposal.value.operation),
                       response->proposal.value.object,
                       endpoint->address().to_string(),
                       endpoint->port(),
                       response->proposal.value.client_once);
            executed_commands.emplace_back(std::move(response));
        }
        executed_cmd_seq++;
        // Try execute the following command until we read a hole
        while(!cmd_min_set.empty()) {
            std::size_t current_wait_sequence = (*cmd_min_set.begin())->sequence;
            if (current_wait_sequence == executed_cmd_seq + 1) {
                std::unique_ptr<Message> next_command = std::move(cmd_min_set.extract(cmd_min_set.begin()).value());
                std::unique_ptr<boost::asio::ip::udp::endpoint> next_endpoint = get_udp_ipv4_endpoint_from_uint64_t(next_command->proposal.value.client_id);
                BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Server {} Execute Command {} {} from Client {}:{} {}\n",
                                                        next_command->sequence, this->get_id(),
                                                        magic_enum::enum_name(next_command->proposal.value.operation),
                                                        next_command->proposal.value.object,
                                                        next_endpoint->address().to_string(),
                                                        next_endpoint->port(),
                                                        next_command->proposal.value.client_once);
                std::unique_ptr<Message> next_response = execute_command(std::move(next_command));
                executed_cmd_seq++;
                if (next_response != nullptr) {
                    BOOST_LOG_TRIVIAL(trace) << fmt::format("Inst Seq {} : Server {} Command Result {} {} to Client {}:{} {}\n",
                                                            next_response->sequence, this->get_id(),
                                                            magic_enum::enum_name(next_response->proposal.value.operation),
                                                            next_response->proposal.value.object,
                                                            next_endpoint->address().to_string(),
                                                            next_endpoint->port(),
                                                            next_response->proposal.value.client_once);
                    executed_commands.emplace_back(std::move(next_response));
                }
            } else {
                break;
            }
        }
    } else if (command->sequence <= executed_cmd_seq) {
        BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Server {} already executed\n",
                                                command->sequence, this->get_id());
    } else {
        // Here is a hole
        BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Server {} cannot executed due to holes exist\n",
                                                command->sequence, this->get_id());
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

