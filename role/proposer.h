//
// Created by Jiacheng Wu on 10/31/22.
//

#ifndef PAXOS_PROPOSER_H
#define PAXOS_PROPOSER_H


#include "message.h"
#include <memory>
#include <mutex>
class Proposer {
  public:

    void set_init_proposal_number_and_number_of_nodes(std::uint32_t server_id, std::uint32_t number_of_nodes) {
        this->current_proposal_number = server_id; // To prevent be 0
        this->number_of_nodes = number_of_nodes;
    }

    std::uint32_t get_next_proposal_number() {
        assert(this->number_of_nodes != 0);
        assert(this->current_proposal_number != 0);

        this->current_proposal_number += number_of_nodes;
        std::uint32_t next_proposal_number = this->current_proposal_number;
        return next_proposal_number;
    }

    // Equals to promise

    std::unique_ptr<Message> propose(std::unique_ptr<Message> submit) {
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
        std::unique_ptr<Message> prepare = std::move(submit);
        prepare->proposal.number = proposal_number;
        return std::move(prepare);
    }

    std::unique_ptr<Message> on_submit(std::unique_ptr<Message> submit) {


        return propose(std::move(submit));
    }

    std::unique_ptr<Message> on_promise(std::unique_ptr<Message> promise) {
        std::lock_guard<std::mutex> lock(proposer_mutex);
        if (promise->proposal.number > this->highest_accepted_proposal_number) {
            this->highest_accepted_proposal_number = promise->proposal.number;
            this->highest_accepted_proposal_value = promise->proposal.value;
        }

        std::unique_ptr<Message> accept = std::move(promise);



        return std::move(accept);
    }

    std::uint32_t get_number_of_promised_acceptors() const {
        return number_of_promised_acceptors;
    }

  private:
    std::mutex proposer_mutex;

    std::uint32_t highest_accepted_proposal_number {0};
    ProposalValue highest_accepted_proposal_value {};
    std::uint32_t number_of_promised_acceptors;

    std::uint32_t current_proposal_number = 0;
    std::uint32_t number_of_nodes = 0;
};


#endif //PAXOS_PROPOSER_H
