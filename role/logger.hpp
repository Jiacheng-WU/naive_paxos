//
// Created by Jiacheng Wu on 11/10/22.
//

#ifndef PAXOS_LOGGER_HPP
#define PAXOS_LOGGER_HPP

/**
 * Logger needs to maintain instances and information,
 * thus it may need to log every operations
 * But it is better to only used in Server granularity.
 * Otherwise
 */

#include <fstream>
#include <vector>
#include "fmt/core.h"
#include "sync.hpp"
#include "message.hpp"

class PaxosServer;



class Logger {
  public:
    Logger(std::filesystem::path acceptor_log_path, std::filesystem::path learner_log_path, PaxosServer* server):
    server(server), acceptor_log_path(acceptor_log_path), learner_log_path(learner_log_path) {
        acceptor_log_file.open(acceptor_log_path, std::ios::out | std::ios::ate | std::ios::binary);
        learner_log_file.open(learner_log_path, std::ios::out | std::ios::ate | std::ios::binary);
        acceptor_log_filesize = std::filesystem::file_size(acceptor_log_path);
        learner_log_filesize = std::filesystem::file_size(learner_log_path);
    }

    inline static constexpr const std::uint32_t basic_file_state_blocks = 1024;
    inline static constexpr const char acceptor_append_buffer[basic_file_state_blocks * AcceptorState::size()] = {};
    inline static constexpr const char learner_append_buffer[basic_file_state_blocks * LearnerState::size()] = {};
    void append_acceptor_as_need(std::uint32_t sequence) {
        if (sequence >= acceptor_log_filesize / sizeof(AcceptorState)) {
            // std::size_t write_start_pos = acceptor_log_filesize;
            std::size_t write_end_pos = (sequence / basic_file_state_blocks + 1) * basic_file_state_blocks * sizeof(AcceptorState);
            acceptor_log_file.seekp(0, std::ios::end);
            acceptor_log_file.write(acceptor_append_buffer, sizeof(acceptor_append_buffer));
            full_sync(acceptor_log_file);
            acceptor_log_filesize = write_end_pos;
        }
    }

    void append_learner_as_need(std::uint32_t sequence) {
        if (sequence >= learner_log_filesize / sizeof(LearnerState)) {
            // std::size_t write_start_pos = acceptor_log_filesize;
            std::size_t write_end_pos = (sequence / basic_file_state_blocks + 1) * basic_file_state_blocks * sizeof(LearnerState);
            learner_log_file.seekp(0, std::ios::end);
            learner_log_file.write(learner_append_buffer, sizeof(learner_append_buffer));
            full_sync(learner_log_file);
            learner_log_filesize = write_end_pos;
        }
    }

    void write_acceptor_log(std::uint32_t sequence, AcceptorState state) {
        append_acceptor_as_need(sequence);
        acceptor_log_file.seekp(sequence * sizeof(AcceptorState));
        state.serialize_to(acceptor_state_buffer);
        acceptor_log_file.write(acceptor_state_buffer, sizeof(acceptor_state_buffer));
        // full_sync(acceptor_log_file) is need, but c++ do not provide it;
        // We just assume follows flush
        full_sync(acceptor_log_file);
    }

    void write_learner_log(std::uint32_t sequence, LearnerState state) {
        append_learner_as_need(sequence);
        learner_log_file.seekp(sequence * sizeof(LearnerState));
        state.serialize_to(learner_state_buffer);
        learner_log_file.write(learner_state_buffer, sizeof(learner_state_buffer));
        // full_sync(acceptor_log_file);
        full_sync(learner_log_file);
    }

    AcceptorState read_acceptor_log(std::uint32_t sequence) {
        acceptor_log_file.seekg(sequence * sizeof(AcceptorState));
        acceptor_log_file.read(acceptor_state_buffer, sizeof(acceptor_state_buffer));
        AcceptorState current_seq_state;
        current_seq_state.deserialize_from(acceptor_state_buffer);
        return current_seq_state;
    }

    LearnerState read_learner_log(std::uint32_t sequence) {
        learner_log_file.seekg(sequence * sizeof(LearnerState));
        learner_log_file.read(learner_state_buffer, sizeof(learner_state_buffer));
        LearnerState current_seq_state;
        current_seq_state.deserialize_from(learner_state_buffer);
        return current_seq_state;
    }

    bool recover_from_log(std::vector<std::uint32_t>& hole_sequence, std::uint32_t& max_sequence);
  private:

    char acceptor_state_buffer[AcceptorState::size()];
    char learner_state_buffer[LearnerState::size()];
    std::filesystem::path acceptor_log_path;
    std::filesystem::path learner_log_path;
    std::fstream acceptor_log_file;
    std::size_t acceptor_log_filesize;
    std::fstream learner_log_file;
    std::size_t learner_log_filesize;
    PaxosServer* server;
};


#endif //PAXOS_LOGGER_HPP
