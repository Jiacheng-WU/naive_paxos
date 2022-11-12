//
// Created by Jiacheng Wu on 10/31/22.
//

#include "proposer.h"
#include "instance.h"
#include "paxos.h"

Proposer::Proposer(Instance *inst) : instance(inst) {
    // We will use the default current_proposal_number for multi-paxos optimization
    this->current_proposal_number = this->instance->server->get_id() + this->instance->server->get_number_of_nodes();
    current_promised_acceptors.reset();
    have_promised = false;
}

std::uint32_t Proposer::get_next_proposal_number() {
    std::lock_guard<std::mutex> lock(proposer_mutex);
    assert(this->number_of_nodes != 0);
    assert(this->current_proposal_number != 0);

    this->current_proposal_number += this->instance->server->get_id();
    std::uint32_t next_proposal_number = this->current_proposal_number;
    return next_proposal_number;
}

std::unique_ptr<Message> Proposer::prepare(std::unique_ptr<Message> submit) {

    if (this->instance->learner.get_learned_majority_consensus()) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(proposer_mutex);

    // As we propose
    this->highest_accepted_proposal_number = 0; // Move to Initial State
    // Why it is safe to set the proposal value as submit one
    this->highest_accepted_proposal_value = submit->proposal.value;
    // Since the promised proposal must have number >= number_of_nodes
    // Thus, any promised proposal value will overwrite with accepted proposal number 0
    // However, if not acceptor ever promised any proposal
    // Then they will just all have 0 as their highest accepted proposal number
    // Therefore, the on_promise would never overwrite this proposal
    // And then we could use it as the proposal value since no acceptor has a value

    // Do propose work
    // ProposalValue value = submit->proposal.value;
    std::uint32_t proposal_number = get_next_proposal_number();
    current_promised_acceptors.reset(); // reset promised acceptor number
    have_promised = false;
    std::unique_ptr<Message> prepare = std::move(submit);
    prepare->proposal.number = proposal_number;
    return std::move(prepare);
}

/*
 * Return nullptr if do nothing
 */
std::unique_ptr<Message> Proposer::on_promise(std::unique_ptr<Message> promise)  {
    std::lock_guard<std::mutex> lock(proposer_mutex);
    // We use prepare_proposal_number to specify the proposal number in PREPARE
    // while the proposal.number is the acceptor's largest number
    // Thus it is impossible promise->prepare_proposal_number > this->current_proposal_number
    assert(promise->prepare_proposal_number <= this->current_proposal_number);
    if (promise->prepare_proposal_number < this->current_proposal_number) {
        return nullptr;
    }


    if (promise->proposal.number > this->highest_accepted_proposal_number) {
        this->highest_accepted_proposal_number = promise->proposal.number;
        this->highest_accepted_proposal_value = promise->proposal.value;
    }

    current_promised_acceptors.set(promise->from_id);

    // Majority
    if(current_promised_acceptors.count() * 2 > this->instance->server->get_number_of_nodes()) {
        std::unique_ptr<Message> accept = std::move(promise);
        accept->type = MessageType::ACCEPT;
        accept->proposal.number = this->current_proposal_number;
        accept->proposal.value = this->highest_accepted_proposal_value;

        accept->from_id = this->instance->server->get_id();
        return std::move(accept);
    } else {
        return nullptr;
    }


}

void Proposer::accept(std::unique_ptr<Message> accept) {

}
