//
// Created by Jiacheng Wu on 10/31/22.
//

#ifndef PAXOS_PROPOSER_H
#define PAXOS_PROPOSER_H


#include "message.h"
#include <memory>
#include <mutex>
#include <bitset>
#include "config.h"
class Instance;

class Proposer {


  private:
    std::uint32_t get_next_proposal_number();
  public:

    Proposer(Instance* inst);

    // Equals to promise

    std::unique_ptr<Message> prepare(std::unique_ptr<Message> submit);

    std::unique_ptr<Message> on_submit(std::unique_ptr<Message> submit) {
        return prepare(std::move(submit));
    }

    /**
     * @param promise
     * @return nullptr represents ignore this out-of-dated promise
     */
    std::unique_ptr<Message> on_promise(std::unique_ptr<Message> promise);
    void accept(std::unique_ptr<Message> accept);

//    std::uint32_t get_current_number_of_promised_acceptors() const {
//        // std::lock_guard<std::mutex> lock(proposer_mutex);
//        return current_number_of_promised_acceptors;
//    }

  private:
    mutable std::mutex proposer_mutex;

    std::uint32_t highest_accepted_proposal_number {0};
    ProposalValue highest_accepted_proposal_value {};

    std::uint32_t current_proposal_number = 0;

    // Avoid Duplicated Message;
    std::bitset<Config::number_of_nodes> current_promised_acceptors {};
    bool have_promised;

    Instance* instance;
};


#endif //PAXOS_PROPOSER_H
