//
// Created by Jiacheng Wu on 10/31/22.
//

#include "learner.hpp"
#include "server.hpp"

Learner::Learner(Instance *inst): instance(inst) {
    current_accepted_acceptors.resize(this->instance->server->get_number_of_nodes(), false);
    learned_majority_consensus = false;
    has_been_informed = false;
}


std::unique_ptr<Message> Learner::on_accepted(std::unique_ptr<Message> accepted) {
    std::lock_guard<std::mutex> lock(learner_mutex);
    /*
     * If we have already learned consensus value from accepted
     */
    if (learned_majority_consensus || has_been_informed || accepted->proposal.number < this->highest_accepted_proposal_number) {
        return nullptr;
    }

    fmt::print("Before on accepted\n");
    if (accepted->proposal.number > this->highest_accepted_proposal_number) {
        this->highest_accepted_proposal_number = accepted->proposal.number;
        this->highest_accepted_proposal_value = accepted->proposal.value;
        this->current_accepted_acceptors.reset();
    }

    // Now accepted->proposal.number > this->highest_accepted_proposal_number
    this->current_accepted_acceptors.set(accepted->from_id);
    if (this->current_accepted_acceptors.count() * 2 > this->instance->server->get_number_of_nodes()) {
        fmt::print("Has Accepted Consensus\n");
        learned_majority_consensus = true;
        std::unique_ptr<Message> inform = std::move(accepted);
        inform->type = MessageType::INFORM;
        inform->proposal.number = this->highest_accepted_proposal_number;
        inform->proposal.value = this->highest_accepted_proposal_value;
        this->instance->deadline_timer.cancel();
        return std::move(inform);
    } else {
        return nullptr;
    }
    // we can then trigger the broadcast INFROM
}

std::unique_ptr<Message> Learner::on_rejected(std::unique_ptr<Message> accepted) {
    return nullptr;
    // we can then trigger the broadcast INFROM
}

void Learner::inform(std::unique_ptr<Message> inform) {
    inform->from_id = this->instance->server->get_id();
    fmt::print("Before inform\n");
    for(std::uint32_t node_id = 0; node_id < this->instance->server->get_number_of_nodes(); node_id++) {
        // We need to clone the unique_ptr<Message> and just send to all nodes;
        std::unique_ptr<Message> inform_copy = inform->clone();
        std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint = this->instance->server->config->get_addr_by_id(node_id);
        this->instance->server->connect->do_send(std::move(inform_copy), std::move(endpoint), do_nothing_handler);
    }
}

std::unique_ptr<Message> Learner::on_inform(std::unique_ptr<Message> inform) {
    fmt::print("Before on inform\n");
    std::lock_guard<std::mutex> lock(learner_mutex);
    // Log State and
//    if (has_been_informed) {
//        return nullptr;
//    }

    has_been_informed = true;
    this->highest_accepted_proposal_number = inform->proposal.number;
    this->highest_accepted_proposal_value = inform->proposal.value;

    this->instance->server->logger->write_learner_log(this->instance->seq,
                                                       {this->instance->seq, this->highest_accepted_proposal_number, this->highest_accepted_proposal_value});
    std::unique_ptr<Message> command = std::move(inform);
    command->type = MessageType::COMMAND;
    return std::move(command);

}

