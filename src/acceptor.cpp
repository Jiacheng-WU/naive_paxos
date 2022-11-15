//
// Created by Jiacheng Wu on 10/31/22.
//

#include "acceptor.hpp"
#include "instance.hpp"
#include "server.hpp"

std::unique_ptr<Message> Acceptor::on_prepare(std::unique_ptr<Message> prepare) {
    std::lock_guard<std::mutex> lock(acceptor_mutex);
    if (this->instance->learner.has_been_informed()) {
        std::size_t prepare_proposal_number = prepare->proposal.number;
        std::unique_ptr<Message> inform = std::move(prepare);
        inform->type = MessageType::INFORM;
        inform->proposal.number = this->instance->learner.get_learned_proposal_number();
        inform->proposal.value = this->instance->learner.get_learned_proposal_value();
        inform->prepare_proposal_number = prepare_proposal_number;
        return std::move(inform);
    }

    if (prepare->proposal.number <= this->highest_prepare_proposal_number) {
        std::size_t prepare_proposal_number = prepare->proposal.number;
        std::unique_ptr<Message> denial = std::move(prepare);
        denial->type = MessageType::DENIAL;
        denial->proposal.number = this->highest_prepare_proposal_number;
        denial->prepare_proposal_number = prepare_proposal_number;
        return std::move(denial);
    } else {

        BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Acceptor {} Prepare Number {}\n",
                                                prepare->sequence,
                                                this->instance->server->get_id(),
                                                prepare->proposal.number);

        // The order is important since we reuse the Message buffer
        this->highest_prepare_proposal_number = prepare->proposal.number;
        if (this->instance->server->config->need_recovery) {
            this->instance->server->logger->write_acceptor_log(this->instance->seq,
                                                               {this->instance->seq,
                                                                this->highest_prepare_proposal_number,
                                                                this->highest_accepted_proposal_number,
                                                                this->highest_accepted_proposal_value});
        }
        // We need to record this promise message reply to which proposal numbered n.
        std::size_t prepare_proposal_number = prepare->proposal.number;
        // Modify Message
        std::unique_ptr<Message> promise = std::move(prepare);
        promise->type = MessageType::PROMISE;

        promise->proposal.number = highest_accepted_proposal_number;
        promise->proposal.value = highest_accepted_proposal_value;
        promise->prepare_proposal_number = prepare_proposal_number;

        BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Acceptor {} Promise Number {} Value {} {} \n",
                                                promise->sequence,
                                                this->instance->server->get_id(),
                                                promise->proposal.number,
                                                magic_enum::enum_name(promise->proposal.value.operation),
                                                promise->proposal.value.object);
        // Do not change from_id here before we get the returned endpoints!!!
        return std::move(promise);
    }
}


std::unique_ptr<Message> Acceptor::on_accept(std::unique_ptr<Message> accept) {
    std::lock_guard<std::mutex> lock(acceptor_mutex);

    if (this->instance->learner.has_been_informed()) {
        std::size_t prepare_proposal_number = accept->proposal.number;
        std::unique_ptr<Message> inform = std::move(accept);
        inform->type = MessageType::INFORM;
        inform->proposal.number = this->instance->learner.get_learned_proposal_number();
        inform->proposal.value = this->instance->learner.get_learned_proposal_value();
        inform->prepare_proposal_number = prepare_proposal_number;
        return std::move(inform);
    }

    if (accept->proposal.number < highest_prepare_proposal_number) {
        std::size_t accept_proposal_number = accept->proposal.number;
        std::unique_ptr<Message> rejected = std::move(accept);
        rejected->type = MessageType::REJECTED;
        rejected->proposal.number = this->highest_prepare_proposal_number;
        rejected->accept_proposal_number = accept_proposal_number;
        return std::move(rejected);
    } else {
        BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Acceptor {} Accept Number {}\n",
                                                accept->sequence,
                                                this->instance->server->get_id(),
                                                accept->proposal.number);

        this->highest_accepted_proposal_number = accept->proposal.number;
        this->highest_accepted_proposal_value = accept->proposal.value;
        if (this->instance->server->config->need_recovery) {
            this->instance->server->logger->write_acceptor_log(this->instance->seq,
                                                               {this->instance->seq,
                                                                this->highest_prepare_proposal_number,
                                                                this->highest_accepted_proposal_number,
                                                                this->highest_accepted_proposal_value});
        }
        std::size_t accept_proposal_number = accept->proposal.number;
        std::unique_ptr<Message> accepted = std::move(accept);
        accepted->type = MessageType::ACCEPTED;
        // Since it is accepted, it is not necessary to modify the proposal value or number
        accepted->accept_proposal_number = accept_proposal_number;
        BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Acceptor {} Accepted Number {} Value {} {} \n",
                                                accepted->sequence,
                                                this->instance->server->get_id(),
                                                accepted->proposal.number,
                                                magic_enum::enum_name(accepted->proposal.value.operation),
                                                accepted->proposal.value.object);
        return std::move(accepted);
    }
}

void Acceptor::promise(std::unique_ptr<Message> promise) {
    BOOST_LOG_TRIVIAL(trace) << fmt::format("Inst Seq {} : Acceptor {} Async Send Promise to Proposer {}\n",
                                            promise->sequence, this->instance->server->get_id(), promise->from_id);
    auto endpoint = this->instance->server->config->get_addr_by_id(promise->from_id);
    promise->from_id = this->instance->server->get_id();
    this->instance->server->connect->do_send(std::move(promise), std::move(endpoint), do_nothing_handler);
}

void Acceptor::denial(std::unique_ptr<Message> denial) {
    BOOST_LOG_TRIVIAL(trace) << fmt::format("Inst Seq {} : Acceptor {} Async Send Denial to Proposer {}\n",
                                            denial->sequence, this->instance->server->get_id(), denial->from_id);
    auto endpoint = this->instance->server->config->get_addr_by_id(denial->from_id);
    denial->from_id = this->instance->server->get_id();
    this->instance->server->connect->do_send(std::move(denial), std::move(endpoint), do_nothing_handler);
}

void Acceptor::accepted(std::unique_ptr<Message> accepted) {
    BOOST_LOG_TRIVIAL(trace) << fmt::format("Inst Seq {} : Acceptor {} Async Send Accepted to Learner {}\n",
                                            accepted->sequence, this->instance->server->get_id(), accepted->from_id);
    auto endpoint = this->instance->server->config->get_addr_by_id(accepted->from_id);
    accepted->from_id = this->instance->server->get_id();
    this->instance->server->connect->do_send(std::move(accepted), std::move(endpoint), do_nothing_handler);
}

void Acceptor::rejected(std::unique_ptr<Message> rejected) {
    BOOST_LOG_TRIVIAL(trace) << fmt::format("Inst Seq {} : Acceptor {} Async Send Rejected to Learner {}\n",
                                            rejected->sequence, this->instance->server->get_id(), rejected->from_id);
    auto endpoint = this->instance->server->config->get_addr_by_id(rejected->from_id);
    rejected->from_id = this->instance->server->get_id();
    this->instance->server->connect->do_send(std::move(rejected), std::move(endpoint), do_nothing_handler);
}

void Acceptor::inform_to_outdated_proposal(std::unique_ptr<Message> inform) {
    BOOST_LOG_TRIVIAL(trace) << fmt::format("Inst Seq {} : Acceptor {} Async Send Inform to Proposer {}\n",
                                            inform->sequence, this->instance->server->get_id(), inform->from_id);
    auto endpoint = this->instance->server->config->get_addr_by_id(inform->from_id);
    inform->from_id = this->instance->server->get_id();
    this->instance->server->connect->do_send(std::move(inform), std::move(endpoint), do_nothing_handler);
}

