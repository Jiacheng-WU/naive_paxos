//
// Created by Jiacheng Wu on 10/31/22.
//

#ifndef PAXOS_PROPOSER_HPP
#define PAXOS_PROPOSER_HPP


#include "message.hpp"
#include <memory>
#include <mutex>
#include <bitset>
#include <boost/dynamic_bitset.hpp>
#include "config.hpp"
class Instance;

class Proposer {

    friend class Instance;
  private:
    std::uint32_t get_next_proposal_number();
  public:

    Proposer(Instance* inst);

    // Equals to promise


    std::unique_ptr<Message> on_submit(std::unique_ptr<Message> submit);
    void prepare(std::unique_ptr<Message> prepare);
    /**
     * @param promise
     * @return nullptr represents ignore this out-of-dated promise
     */
    std::unique_ptr<Message> on_promise(std::unique_ptr<Message> promise);
    std::unique_ptr<Message> on_denial(std::unique_ptr<Message> promise);
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
    boost::dynamic_bitset<std::uint8_t> current_promised_acceptors;
    boost::dynamic_bitset<std::uint8_t> current_denied_acceptors;
    bool have_promised;

    Instance* instance;
};


#endif //PAXOS_PROPOSER_HPP
