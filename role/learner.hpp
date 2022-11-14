//
// Created by Jiacheng Wu on 10/31/22.
//

#ifndef PAXOS_LEARNER_HPP
#define PAXOS_LEARNER_HPP


#include <mutex>
#include <boost/dynamic_bitset.hpp>
#include "message.hpp"
#include "config.hpp"

class Instance;

class Learner {
  public:

    Learner(Instance* inst);

    void recover_from_state(LearnerState state) {
        this->highest_accepted_proposal_number = state.final_informed_proposal_number;
        this->highest_accepted_proposal_value = state.final_informed_proposal_value;
        learned_majority_consensus = true;
        has_been_informed = true;
    }
    // For distinguished learner
    std::unique_ptr<Message> on_accepted(std::unique_ptr<Message> accepted);

    std::unique_ptr<Message> on_rejected(std::unique_ptr<Message> rejected);

    void inform(std::unique_ptr<Message> inform);

    std::unique_ptr<Message> on_inform(std::unique_ptr<Message> inform);

    std::unique_ptr<Message> on_learn(std::unique_ptr<Message> learn) {
        return std::move(learn);
    }

    bool get_learned_majority_consensus() {
        // std::lock_guard<std::mutex> lock(learner_mutex);
        return learned_majority_consensus;
    }
  private:

    mutable std::mutex learner_mutex;

    std::uint32_t highest_accepted_proposal_number {0};
    ProposalValue highest_accepted_proposal_value {};
    boost::dynamic_bitset<std::uint8_t> current_accepted_acceptors;
    bool learned_majority_consensus;
    bool has_been_informed;

    Instance* instance;
};


#endif //PAXOS_LEARNER_HPP
