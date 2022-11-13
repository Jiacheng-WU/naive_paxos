//
// Created by Jiacheng Wu on 11/12/22.
//

#include "client.h"

std::unique_ptr<Message>
PaxosClient::construct_lock_or_unlock_message(std::uint32_t object_id, PaxosClient::op_type op) {
    std::unique_ptr<Message> lock_request = std::make_unique<Message>();
    lock_request->type = MessageType::SUBMIT;
    if (op == op_type::LOCK) {lock_request->proposal.value.operation = ProposalValue::LOCK;}
    else if (op == op_type::UNLOCK) {lock_request->proposal.value.operation = ProposalValue::UNLOCK;}
    else {assert("Failed to identify op type");}
    lock_request->proposal.value.object = object_id;
    client_op_id++;
    lock_request->proposal.value.client_once = client_op_id;
    lock_request->proposal.value.client_id = 0;
    // lock_request->client_once = client_op_id;
    return std::move(lock_request);
}

PaxosClient::op_result_type PaxosClient::lock_or_unlock(std::uint32_t object_id, PaxosClient::op_type op)  {
    std::unique_ptr<Message> submit = construct_lock_or_unlock_message(object_id, op);
    submit->serialize_to(out_message);

    socket.send_to(boost::asio::buffer(out_message), get_server_endpoint(leader_server_id));

    boost::asio::ip::udp::endpoint udp_endpoint;
    socket.receive_from(boost::asio::buffer(in_message), udp_endpoint);
    std::unique_ptr<Message> response_or_redirect = std::make_unique<Message>();
    response_or_redirect->deserialize_from(in_message);
    if (response_or_redirect->type == MessageType::REDIRECT) {
        leader_server_id = response_or_redirect->leader_id;
        socket.send_to(boost::asio::buffer(out_message), get_server_endpoint(leader_server_id));
        socket.receive_from(boost::asio::buffer(in_message), udp_endpoint);
        response_or_redirect->deserialize_from(in_message);
    }
    while (response_or_redirect->proposal.value.client_once < client_op_id) {
        socket.receive_from(boost::asio::buffer(in_message), udp_endpoint);
        response_or_redirect->deserialize_from(in_message);
    }
    return translate_message_operation_to_result(response_or_redirect->proposal.value.operation);
}
