//
// Created by Jiacheng Wu on 11/12/22.
//

#ifndef PAXOS_CLIENT_HPP
#define PAXOS_CLIENT_HPP

#include <boost/asio.hpp>
#include "config.hpp"
#include "network.hpp"
#include "message.hpp"
#include "sync.hpp"
#include <fstream>
#include <cstring>

class PaxosClient {
  public:
    PaxosClient(std::unique_ptr<Config> config, std::uint16_t port = 0):
        config(std::move(config)),
        socket(io_context,
               boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)){
        local_port = socket.local_endpoint().port();
        BOOST_LOG_TRIVIAL(info) << fmt::format("The Clients port number is {}\n", local_port);
        client_op_id = 0;
        if (config->at_most_once) {
            auto client_log_path = config->get_client_file_path(local_port);
            if (!std::filesystem::exists(client_log_path)) {
                client_log_file.open(client_log_path, std::ios::out | std::ios::binary);
                client_log_file.close();
                client_log_file.open(client_log_path, std::ios::out | std::ios::in | std::ios::binary);
                write_client_op_id(client_op_id);
                client_log_file.close();
            }
            client_log_file.open(client_log_path, std::ios::out | std::ios::in | std::ios::binary);
            client_op_id = read_client_op_id();
        }
    }

    ~PaxosClient() {
        if (config->at_most_once) {
            client_log_file.close();
        }
    }

    static void handle_receive(
            const boost::system::error_code& error, std::size_t length,
            boost::system::error_code* out_error, std::size_t* out_length)
    {
        *out_error = error;
        *out_length = length;
    }

    void run(std::chrono::steady_clock::duration timeout)
    {
        // Restart the io_context, as it may have been left in the "stopped" state
        // by a previous operation.
        io_context.restart();

        // Block until the asynchronous operation has completed, or timed out. If
        // the pending asynchronous operation is a composed operation, the deadline
        // applies to the entire operation, rather than individual operations on
        // the socket.
        io_context.run_for(timeout);

        // If the asynchronous operation completed successfully then the io_context
        // would have been stopped due to running out of work. If it was not
        // stopped, then the io_context::run_for call must have timed out.
        if (!io_context.stopped())
        {
            // Cancel the outstanding asynchronous operation.
            socket.cancel();

            // Run the io_context again until the operation completes.
            io_context.run();
        }
    }

    std::size_t receive(const boost::asio::mutable_buffer& buffer,
                        std::chrono::steady_clock::duration timeout,
                        boost::system::error_code& error)
    {
        // Start the asynchronous operation. The handle_receive function used as a
        // callback will update the error and length variables.
        std::size_t length = 0;
        socket.async_receive(boost::asio::buffer(buffer),
                              std::bind(&PaxosClient::handle_receive, std::placeholders::_1, std::placeholders::_2, &error, &length));

        // Run the operation until it completes, or until the timeout.
        run(timeout);

        return length;
    }


    boost::asio::ip::udp::endpoint get_next_server_endpoint() {
        server_id ++;
        server_id %= config->number_of_nodes;
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
                return op_result_type::LOCK_FAILED;
            case ProposalValue::LOCK_AGAIN:
                return op_result_type::LOCK_AGAIN;
            case ProposalValue::UNLOCK_FAILED:
                return op_result_type::UNLOCK_FAILED;
            case ProposalValue::UNLOCK_RELEASED:
                return op_result_type::UNLOCK_AGAIN;
            default:
                return op_result_type::UNDEFINED;
        }
    }

    std::unique_ptr<Message> construct_lock_or_unlock_message(std::uint32_t object_id, op_type op);

    std::pair<op_result_type, std::uint32_t> lock_or_unlock(std::uint32_t object_id, op_type op);


    std::pair<op_result_type, std::uint32_t> lock(std::uint32_t object_id) {
        return lock_or_unlock(object_id, op_type::LOCK);
    }

    std::pair<op_result_type, std::uint32_t> unlock(std::uint32_t object_id) {
        return lock_or_unlock(object_id, op_type::UNLOCK);
    }

//  private:
    std::uint8_t out_message[Message::size()];
    std::uint8_t in_message[Message::size()];
    boost::asio::io_context io_context;
    boost::asio::ip::udp::socket socket;
    std::unique_ptr<Config> config;
    std::uint32_t client_op_id = 0;
    std::uint32_t server_id = 0;
    std::uint16_t local_port = 0;
    std::fstream client_log_file;


    char client_op_id_buffer[sizeof(uint32_t)] = {};

    void write_client_op_id(std::uint32_t temp_client_op_id) {
        memcpy(client_op_id_buffer, &temp_client_op_id, sizeof(std::uint32_t));
        client_log_file.seekp(0);
        client_log_file.write(client_op_id_buffer, sizeof(std::uint32_t));
        full_sync(client_log_file);
    }
    std::uint32_t read_client_op_id() {
        client_log_file.seekg(0);
        client_log_file.read(client_op_id_buffer, sizeof(std::uint32_t));
        std::uint32_t temp_client_op_id = 0;
        memcpy(&temp_client_op_id, client_op_id_buffer, sizeof(std::uint32_t));
        return temp_client_op_id;
    }

};


#endif //PAXOS_CLIENT_HPP
