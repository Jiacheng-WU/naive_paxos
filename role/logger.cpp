//
// Created by Jiacheng Wu on 11/10/22.
//

#include "logger.h"
#include "server.h"
bool Logger::recover_from_log(std::vector<std::uint32_t> &hole_sequence, uint32_t &max_sequence)  {
    if (acceptor_log_filesize == 0 && learner_log_filesize == 0) {
        return false;
    }
    acceptor_log_file.close();
    learner_log_file.close();
    acceptor_log_file.open(acceptor_log_path, std::ios::in | std::ios::binary);
    learner_log_file.open(learner_log_path, std::ios::in | std::ios::binary);

    acceptor_log_filesize = std::filesystem::file_size(acceptor_log_path);
    learner_log_filesize = std::filesystem::file_size(learner_log_path);

    std::uint32_t acceptor_log_items = acceptor_log_filesize/sizeof(AcceptorState);
    std::uint32_t learner_log_items = learner_log_filesize/sizeof(LearnerState);


    for(std::uint32_t i = 1; i < acceptor_log_items; i++) {
        AcceptorState state = read_acceptor_log(i);
        if (state.sequence == i) {
            this->server->instances.get_instance(i)->acceptor.recover_from_state(state);
        }
    }

    max_sequence = 0;
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

    acceptor_log_file.close();
    learner_log_file.close();
    acceptor_log_file.open(acceptor_log_path, std::ios::out | std::ios::ate | std::ios::binary);
    learner_log_file.open(learner_log_path, std::ios::out | std::ios::ate | std::ios::binary);
    return true;
}
