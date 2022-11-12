//
// Created by Jiacheng Wu on 10/31/22.
//

#include "learner.h"
#include "paxos.h"
std::unique_ptr<Message> Learner::on_accepted(std::unique_ptr<Message> accepted) {
    std::lock_guard<std::mutex> lock(learner_mutex);
    /*
     * If we have already learned consensus value from accepted
     */
    if (learned_majority_consensus || accepted->proposal.number < this->highest_accepted_proposal_number) {
        return nullptr;
    }

    if (accepted->proposal.number > this->highest_accepted_proposal_number) {
        this->highest_accepted_proposal_number = accepted->proposal.number;
        this->current_accepted_acceptors.reset();
    }

    // Now accepted->proposal.number > this->highest_accepted_proposal_number
    this->current_accepted_acceptors.set(accepted->from_id);
    if (this->current_accepted_acceptors.count() * 2 > this->instance->server->get_number_of_nodes()) {
        learned_majority_consensus = true;
        std::unique_ptr<Message> inform = std::move(accepted);
        inform->type = MessageType::INFORM;
        return std::move(inform);
    } else {
        return nullptr;
    }

    // we can then trigger the broadcast INFROM
}

