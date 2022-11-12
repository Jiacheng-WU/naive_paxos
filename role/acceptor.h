//
// Created by Jiacheng Wu on 10/31/22.
//

#ifndef PAXOS_ACCEPTOR_H
#define PAXOS_ACCEPTOR_H

#include "message.h"
#include <memory>
#include <mutex>

class Instance;

class Acceptor {
  public:

    Acceptor(Instance* inst):instance(inst) {}

    std::unique_ptr<Message> on_prepare(std::unique_ptr<Message> prepare);

    void promise(std::unique_ptr<Message> promise);
    void denial(std::unique_ptr<Message> denial);
    void promise_or_denial(std::unique_ptr<Message> promise_or_denial) {
        if (promise_or_denial->type == MessageType::PROMISE) {
            this->promise(std::move(promise_or_denial));
        } else if (promise_or_denial->type == MessageType::DENIAL) {
            this->denial(std::move(promise_or_denial));
        } else {
            assert((promise_or_denial->type == MessageType::PROMISE || promise_or_denial->type == MessageType::DENIAL));
        }
    }
    std::unique_ptr<Message> on_accept(std::unique_ptr<Message> accept);

    void accepted(std::unique_ptr<Message> accepted);
    void rejected(std::unique_ptr<Message> rejected);

    void accepted_or_reject(std::unique_ptr<Message> accepted_or_rejected) {
        if(accepted_or_rejected->type == MessageType::ACCEPTED) {
            this->accepted(std::move(accepted_or_rejected));
        } else if (accepted_or_rejected->type == MessageType::REJECTED){
            this->rejected(std::move(accepted_or_rejected));
        } else {
            assert((accepted_or_rejected->type == MessageType::ACCEPTED || accepted_or_rejected->type == MessageType::REJECTED));
        }
    }


  private:
    mutable std::mutex acceptor_mutex;
    std::uint32_t highest_prepare_proposal_number {0};
    std::uint32_t highest_accepted_proposal_number {0};
    ProposalValue highest_accepted_proposal_value {};

    Instance* instance;
};


#endif //PAXOS_ACCEPTOR_H
