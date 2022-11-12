//
// Created by Jiacheng Wu on 10/31/22.
//

#ifndef PAXOS_LEARNER_H
#define PAXOS_LEARNER_H


#include <mutex>
#include "message.h"
#include "config.h"

class Instance;

class Learner {
  public:

    Learner(Instance* inst):instance(inst) {}

    // For distinguished learner
    std::unique_ptr<Message> on_accepted(std::unique_ptr<Message> accepted);

    std::unique_ptr<Message> on_inform(std::unique_ptr<Message> inform) {
        return std::move(inform);
    }

    std::unique_ptr<Message> on_learn(std::unique_ptr<Message> learn) {
        return std::move(learn);
    }

    bool get_learned_majority_consensus() {
        std::lock_guard<std::mutex> lock(learner_mutex);
        return learned_majority_consensus;
    }
  private:

    mutable std::mutex learner_mutex;

    std::uint32_t highest_accepted_proposal_number {0};
    ProposalValue highest_accepted_proposal_value {};
    std::bitset<Config::number_of_nodes> current_accepted_acceptors {};
    bool learned_majority_consensus;

    Instance* instance;
};


#endif //PAXOS_LEARNER_H
