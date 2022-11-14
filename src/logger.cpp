//
// Created by Jiacheng Wu on 11/10/22.
//

#include "logger.hpp"
#include "server.hpp"
bool Logger::recover_from_log(std::vector<std::uint32_t> &hole_sequence, uint32_t& min_sequence, uint32_t &max_sequence)  {

    if (acceptor_log_filesize == 0 && learner_log_filesize == 0)
        return false;

    std::uint32_t acceptor_log_items = acceptor_log_filesize/sizeof(AcceptorState);
    std::uint32_t learner_log_items = learner_log_filesize/sizeof(LearnerState);

    BOOST_LOG_TRIVIAL(info) << fmt::format("Server {} : Acceptor Log size {} item {}\n", this->server->get_id(), acceptor_log_filesize, acceptor_log_items);
    BOOST_LOG_TRIVIAL(info) << fmt::format("Server {} : Learner Log size {} item {}\n", this->server->get_id(), learner_log_filesize, learner_log_items);

    for(std::uint32_t i = 1; i < acceptor_log_items; i++) {
        AcceptorState state = read_acceptor_log(i);
        if (state.sequence == i) {
            this->server->instances.get_instance(i)->acceptor.recover_from_state(state);
        }
    }

    min_sequence = 0;
    max_sequence = 0;

    for(std::uint32_t i = 1; i < learner_log_items; i++) {
        LearnerState state = read_learner_log(i);
        if (state.sequence != i) {
            min_sequence = i - 1;
            break;
        }
    }

    std::uint32_t last_sequence = 0;
    for(std::uint32_t i = 1; i < learner_log_items; i++) {
        LearnerState state = read_learner_log(i);
        if (state.sequence == i) {
            this->server->instances.get_instance(i)->learner.recover_from_state(state);
            max_sequence = i;
            for(last_sequence++; last_sequence < max_sequence; last_sequence++) {
                hole_sequence.push_back(last_sequence);
            }
        } else {
            last_sequence = max_sequence;
        }
    }
    return true;
}
