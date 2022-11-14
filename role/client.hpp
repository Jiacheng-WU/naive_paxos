//
// Created by Jiacheng Wu on 11/12/22.
//

#ifndef PAXOS_CLIENT_HPP
#define PAXOS_CLIENT_HPP

#include <boost/asio.hpp>
#include "config.hpp"
#include "network.hpp"
#include "message.hpp"


class client
{
  public:
    client(const boost::asio::ip::udp::endpoint& listen_endpoint)
            : socket_(io_context_, listen_endpoint)
    {
    }

    std::size_t receive(const boost::asio::mutable_buffer& buffer,
                        std::chrono::steady_clock::duration timeout,
                        boost::system::error_code& error)
    {
        // Start the asynchronous operation. The handle_receive function used as a
        // callback will update the error and length variables.
        std::size_t length = 0;
        socket_.async_receive(boost::asio::buffer(buffer),
                              std::bind(&client::handle_receive, std::placeholders::_1, std::placeholders::_2, &error, &length));

        // Run the operation until it completes, or until the timeout.
        run(timeout);

        return length;
    }

  private:
    void run(std::chrono::steady_clock::duration timeout)
    {
        // Restart the io_context, as it may have been left in the "stopped" state
        // by a previous operation.
        io_context_.restart();

        // Block until the asynchronous operation has completed, or timed out. If
        // the pending asynchronous operation is a composed operation, the deadline
        // applies to the entire operation, rather than individual operations on
        // the socket.
        io_context_.run_for(timeout);

        // If the asynchronous operation completed successfully then the io_context
        // would have been stopped due to running out of work. If it was not
        // stopped, then the io_context::run_for call must have timed out.
        if (!io_context_.stopped())
        {
            // Cancel the outstanding asynchronous operation.
            socket_.cancel();

            // Run the io_context again until the operation completes.
            io_context_.run();
        }
    }

    static void handle_receive(
            const boost::system::error_code& error, std::size_t length,
            boost::system::error_code* out_error, std::size_t* out_length)
    {
        *out_error = error;
        *out_length = length;
    }

  private:
    boost::asio::io_context io_context_;
    boost::asio::ip::udp::socket socket_;
};

class PaxosClient {
  public:
    PaxosClient(std::unique_ptr<Config> config):
        config(std::move(config)),
        socket(io_context,
               boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0))
                                              {}
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
        server_id %= config->get_number_of_nodes();
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

    op_result_type lock_or_unlock(std::uint32_t object_id, op_type op);


    op_result_type lock(std::uint32_t object_id) {
        return lock_or_unlock(object_id, op_type::LOCK);
    }

    op_result_type unlock(std::uint32_t object_id) {
        return lock_or_unlock(object_id, op_type::UNLOCK);
    }

//  private:
    uint8_t out_message[Message::size()];
    uint8_t in_message[Message::size()];
    boost::asio::io_context io_context;
    boost::asio::ip::udp::socket socket;
    std::unique_ptr<Config> config;
    std::uint32_t client_op_id = 0;
    std::uint32_t server_id = 0;
};


#endif //PAXOS_CLIENT_HPP
