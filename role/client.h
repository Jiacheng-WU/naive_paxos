//
// Created by Jiacheng Wu on 11/12/22.
//

#ifndef PAXOS_CLIENT_H
#define PAXOS_CLIENT_H

#include <boost/asio.hpp>
#include "config.h"
#include "network.h"
#include "message.h"
class PaxosClient {
  public:
    PaxosClient(boost::asio::io_context& io_context, std::unique_ptr<Config> config):
        config(std::move(config)),
        socket(io_context,
               boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0))
                                              {}



    boost::asio::ip::udp::endpoint get_server_endpoint(std::uint32_t server_id) {
        return config->server_id_to_addr_map[server_id];
    }

    enum class op_type {
        LOCK,
        UNLOCK
    };

    enum class op_result_type {
        LOCK_SUCCEED,
        UNLOCK_SUCCEED,
        LOCK_FAILED, // have been previously locked by other clients
        LOCK_AGAIN, // have been previously locked by itself
        UNLOCK_FAILED, // have been not previous locked
        UNLOCK_AGAIN, // unlock on a already unlocked object
        UNDEFINED,
    };

    op_result_type translate_message_operation_to_result(ProposalValue::ProposalOperation result) {
        switch (result) {
            case ProposalValue::LOCK_SUCCEED:
                return op_result_type::LOCK_SUCCEED;
            case ProposalValue::UNLOCK_SUCCEED:
                return op_result_type::UNLOCK_SUCCEED;
            case ProposalValue::LOCK_FAILED:
                return op_result_type::LOCK_SUCCEED;
            case ProposalValue::LOCK_AGAIN:
                return op_result_type::LOCK_AGAIN;
            case ProposalValue::UNLOCK_FAILED:
                return op_result_type::UNLOCK_FAILED;
            case ProposalValue::UNLOCK_AGAIN:
                return op_result_type::UNLOCK_AGAIN;
            default:
                return op_result_type::UNDEFINED;
        }
    }

    std::unique_ptr<Message> construct_lock_or_unlock_message(std::uint32_t object_id, op_type op);

    op_result_type lock_or_unlock(std::uint32_t object_id, op_type op);


    op_result_type lock(std::uint32_t object_id) {
        return lock_or_unlock(object_id, op_type::LOCK);
    }

    op_result_type unlock(std::uint32_t object_id) {
        return lock_or_unlock(object_id, op_type::LOCK);
    }

//  private:
    uint8_t out_message[Message::size()];
    uint8_t in_message[Message::size()];
    boost::asio::ip::udp::socket socket;
    std::unique_ptr<Config> config;
    std::uint32_t client_op_id = 0;
    std::uint32_t leader_server_id = 0;
};


#endif //PAXOS_CLIENT_H
