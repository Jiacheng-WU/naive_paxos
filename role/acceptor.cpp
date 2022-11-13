//
// Created by Jiacheng Wu on 10/31/22.
//

#include "acceptor.h"
#include "instance.h"
#include "server.h"

std::unique_ptr<Message> Acceptor::on_prepare(std::unique_ptr<Message> prepare) {
    fmt::print("Accept instance {} proposal {}\n", this->instance->seq, prepare->proposal.number);
    std::lock_guard<std::mutex> lock(acceptor_mutex);
    if (prepare->proposal.number <= this->highest_prepare_proposal_number) {
        std::size_t prepare_proposal_number = prepare->proposal.number;
        std::unique_ptr<Message> denial = std::move(prepare);
        denial->type = MessageType::DENIAL;
        denial->proposal.number = this->highest_prepare_proposal_number;
        denial->prepare_proposal_number = prepare_proposal_number;
        // Do not influence the value;
        // denial->from_id = this->instance->server->get_id();
        return std::move(denial);
    } else {
        // The order is important since we reuse the Message buffer
        this->highest_prepare_proposal_number = prepare->proposal.number;
        this->instance->server->logger->write_acceptor_log(this->instance->seq,
            {this->instance->seq, this->highest_prepare_proposal_number, this->highest_accepted_proposal_number, this->highest_accepted_proposal_value});
        // We need to record this promise message reply to which proposal numbered n.
        std::size_t prepare_proposal_number = prepare->proposal.number;
        // Modify Message
        std::unique_ptr<Message> promise = std::move(prepare);
        promise->type = MessageType::PROMISE;

        promise->proposal.number = highest_accepted_proposal_number;
        promise->proposal.value = highest_accepted_proposal_value;
        promise->prepare_proposal_number = prepare_proposal_number;
        // promise->from_id = this->instance->server->get_id();
        return std::move(promise);
    }
}


std::unique_ptr<Message> Acceptor::on_accept(std::unique_ptr<Message> accept) {
    std::lock_guard<std::mutex> lock(acceptor_mutex);
    if(accept->proposal.number < highest_prepare_proposal_number) {
        std::size_t accept_proposal_number = accept->proposal.number;
        std::unique_ptr<Message> rejected = std::move(accept);
        rejected->type = MessageType::REJECTED;
        rejected->proposal.number = this->highest_prepare_proposal_number;
        rejected->accept_proposal_number = accept_proposal_number;
        return std::move(rejected);
    } else {
        this->highest_accepted_proposal_number = accept->proposal.number;
        this->highest_accepted_proposal_value = accept->proposal.value;

        this->instance->server->logger->write_acceptor_log(this->instance->seq,
             {this->instance->seq, this->highest_prepare_proposal_number, this->highest_accepted_proposal_number, this->highest_accepted_proposal_value});

        std::size_t accept_proposal_number = accept->proposal.number;
        std::unique_ptr<Message> accepted = std::move(accept);
        accept->type = MessageType::ACCEPTED;
        // Since it is accepted, it is not necessary to modify the proposal value or number
        accepted->accept_proposal_number = accept_proposal_number;
        return std::move(accepted);
    }
}

void Acceptor::promise(std::unique_ptr<Message> promise) {
    auto endpoint = this->instance->server->config->get_addr_by_id(promise->from_id);
    promise->from_id = this->instance->server->get_id();
    this->instance->server->connect->do_send(std::move(promise), std::move(endpoint), do_nothing_handler);
}

void Acceptor::denial(std::unique_ptr<Message> denial) {
    auto endpoint = this->instance->server->config->get_addr_by_id(denial->from_id);
    denial->from_id = this->instance->server->get_id();
    this->instance->server->connect->do_send(std::move(denial), std::move(endpoint), do_nothing_handler);
}

void Acceptor::accepted(std::unique_ptr<Message> accepted) {
    auto endpoint = this->instance->server->config->get_addr_by_id(accepted->from_id);
    accepted->from_id = this->instance->server->get_id();
    this->instance->server->connect->do_send(std::move(accepted), std::move(endpoint),
                                                  do_nothing_handler);
}

void Acceptor::rejected(std::unique_ptr<Message> rejected) {
    auto endpoint = this->instance->server->config->get_addr_by_id(rejected->from_id);
    rejected->from_id = this->instance->server->get_id();
    this->instance->server->connect->do_send(std::move(rejected), std::move(endpoint),
                                                  do_nothing_handler);

}
