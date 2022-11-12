//
// Created by Jiacheng Wu on 11/10/22.
//

#ifndef PAXOS_LOGGER_H
#define PAXOS_LOGGER_H

/**
 * Logger needs to maintain instances and information,
 * thus it may need to log every operations
 * But it is better to only used in Server granularity.
 * Otherwise
 */

#include <fstream>
#include <fmt/core.h>
#include "sync.h"
#include "message.h"
class Logger {
  public:
    Logger(std::filesystem::path file_path) {
        log_file.open(file_path, std::ios::in | std::ios::out | std::ios::binary);
    }

    void write_message(Message& m) {
        m.serialize_to(out_message_buffer);
        // log_file.
    }

    void read_message(Message& m) {
        m.deserialize_from(in_message_buffer);
    }
  private:

    char out_message_buffer[Message::size()];
    char in_message_buffer[Message::size()];
    std::fstream log_file;
};


#endif //PAXOS_LOGGER_H
