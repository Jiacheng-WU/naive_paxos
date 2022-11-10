//
// Created by Jiacheng Wu on 10/31/22.
//

#ifndef PAXOS_ACCEPTOR_H
#define PAXOS_ACCEPTOR_H

#include "message.h"
#include <memory>
#include <mutex>
class Acceptor {
  public:


    std::unique_ptr<Message> on_prepare(std::unique_ptr<Message> prepare) {
        std::lock_guard<std::mutex> lock(acceptor_mutex);
        if (prepare->proposal.number <= this->highest_prepare_proposal_number) {
            std::size_t prepare_proposal_number = prepare->proposal.number;
            std::unique_ptr<Message> denial = std::move(prepare);
            denial->type = MessageType::DENIAL;
            denial->proposal.number = this->highest_prepare_proposal_number;
            denial->prepare_proposal_number = prepare_proposal_number;
            return std::move(denial);
        } else {
            // The order is important since we reuse the Message buffer
            this->highest_prepare_proposal_number = prepare->proposal.number;
            // We need to record this promise message reply to which proposal numbered n.
            std::size_t prepare_proposal_number = prepare->proposal.number;
            // Modify Message
            std::unique_ptr<Message> promise = std::move(prepare);
            promise->type = MessageType::PROMISE;

            promise->proposal.number = highest_accepted_proposal_number;
            promise->proposal.value = highest_accepted_proposal_value;
            promise->prepare_proposal_number = prepare_proposal_number;

            return std::move(promise);
        }
    }

    std::unique_ptr<Message> on_accept(std::unique_ptr<Message> accept) {
        std::lock_guard<std::mutex> lock(acceptor_mutex);
        if(accept->proposal.number < highest_prepare_proposal_number) {
            std::size_t accept_proposal_number = accept->proposal.number;
            std::unique_ptr<Message> rejected = std::move(accept);
            rejected->type = MessageType::REJECTED;
            rejected->proposal.number = this->highest_prepare_proposal_number;
            rejected->accept_proposal_number = accept_proposal_number;
            return std::move(rejected);
        } else {
            this->highest_accepted_proposal_number = accept->proposal.number;
            this->highest_accepted_proposal_value = accept->proposal.value;
            std::size_t accept_proposal_number = accept->proposal.number;

            std::unique_ptr<Message> accepted = std::move(accept);
            accept->type = MessageType::ACCEPTED;
            // Since it is accepted, it is not necessary to modify the proposal value or number
            accepted->accept_proposal_number = accept_proposal_number;

            return std::move(accepted);
        }
    }


  private:
    std::mutex acceptor_mutex;
    std::uint32_t highest_prepare_proposal_number {0};
    std::uint32_t highest_accepted_proposal_number {0};
    ProposalValue highest_accepted_proposal_value {};

};


#endif //PAXOS_ACCEPTOR_H
