//
// Created by Jiacheng Wu on 10/31/22.
//

#include "proposer.hpp"
#include "instance.hpp"
#include "server.hpp"
#include "config.hpp"

Proposer::Proposer(Instance *inst) : instance(inst) {
    // We will use the default current_proposal_number for multi-paxos optimization
    this->current_proposal_number = this->instance->server->get_id() + this->instance->server->get_number_of_nodes();
    // We now use dynamic bitset
    current_promised_acceptors.resize(this->instance->server->get_number_of_nodes(), false);
    current_denied_acceptors.resize(this->instance->server->get_number_of_nodes(), false);
    have_promised = false;
}

std::uint32_t Proposer::get_next_proposal_number() {
    assert(this->instance->server->get_number_of_nodes() != 0);
    assert(this->current_proposal_number != 0);

    this->current_proposal_number += this->instance->server->get_number_of_nodes();
    std::uint32_t next_proposal_number = this->current_proposal_number;
    return next_proposal_number;
}

// submit->seq is set before
std::unique_ptr<Message> Proposer::on_submit(std::unique_ptr<Message> submit) {

    if (this->instance->learner.get_learned_majority_consensus() ||
        this->instance->learner.has_been_informed()) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(proposer_mutex);
    BOOST_LOG_TRIVIAL(trace) << fmt::format("Inst Seq {} : Propose {} - on_submit\n",
                                            submit->sequence, this->instance->server->get_id());

    // As we propose
    this->highest_accepted_proposal_number = 0; // Move to Initial State
    // Why it is safe to set the proposal value as submit one
    this->highest_accepted_proposal_value = submit->proposal.value;
    // Since the promised proposal must have number >= number_of_nodes
    // Thus, any promised proposal value will overwrite with accepted proposal number 0
    // However, if not acceptor ever promised any proposal
    // Then they will just all have 0 as their highest accepted proposal number
    // Therefore, the on_promise would never overwrite this proposal
    // And then we could use it as the proposal value since no acceptor has a value

    // Do propose work
    // ProposalValue value = submit->proposal.value;
    std::uint32_t proposal_number = get_next_proposal_number();
    current_promised_acceptors.reset(); // reset promised acceptor number
    current_denied_acceptors.reset();
    have_promised = false;
    std::unique_ptr<Message> prepare = std::move(submit);
    prepare->type = MessageType::PREPARE;
    prepare->proposal.number = proposal_number;
    return std::move(prepare);
}

/*
 * Return nullptr if do nothing
 */
std::unique_ptr<Message> Proposer::on_promise(std::unique_ptr<Message> promise)  {
    BOOST_LOG_TRIVIAL(trace) << fmt::format("Inst Seq {} : Proposer {} - on_promise\n",
                                            promise->sequence, this->instance->server->get_id());

    std::lock_guard<std::mutex> lock(proposer_mutex);
    // We use prepare_proposal_number to specify the proposal number in PREPARE
    // while the proposal.number is the acceptor's largest number
    // Thus it is impossible promise->prepare_proposal_number > this->current_proposal_number
    assert(promise->prepare_proposal_number <= this->current_proposal_number);
    if (promise->prepare_proposal_number < this->current_proposal_number) {
        return nullptr;
    }


    if (promise->proposal.number > this->highest_accepted_proposal_number) {
        this->highest_accepted_proposal_number = promise->proposal.number;
        this->highest_accepted_proposal_value = promise->proposal.value;
    }

    current_promised_acceptors.set(promise->from_id);
    // Majority
    if(current_promised_acceptors.count() * 2 > this->instance->server->get_number_of_nodes()) {
        have_promised = true;
        std::unique_ptr<Message> accept = std::move(promise);
        accept->type = MessageType::ACCEPT;
        accept->proposal.number = this->current_proposal_number;
        accept->proposal.value = this->highest_accepted_proposal_value;
        // accept->from_id = this->instance->server->get_id();
        this->instance->deadline_timer.cancel();

        return std::move(accept);
    } else {
        return nullptr;
    }
}

std::unique_ptr<Message> Proposer::on_denial(std::unique_ptr<Message> denial)  {
    BOOST_LOG_TRIVIAL(trace) << fmt::format("Inst Seq {} : Proposer {} - on_denial\n",
                                            denial->sequence, this->instance->server->get_id());

    std::lock_guard<std::mutex> lock(proposer_mutex);
    // We use prepare_proposal_number to specify the proposal number in PREPARE
    // while the proposal.number is the acceptor's largest number
    // Thus it is impossible promise->prepare_proposal_number > this->current_proposal_number
    assert(denial->prepare_proposal_number <= this->current_proposal_number);
    if (denial->prepare_proposal_number < this->current_proposal_number) {
        return nullptr;
    }

    current_denied_acceptors.set(denial->from_id);

    // Majority
    if(current_denied_acceptors.count() * 2 > this->instance->server->get_number_of_nodes()) {
        std::unique_ptr<Message> resubmit = std::move(denial);
        resubmit->type = MessageType::SUBMIT;
        resubmit->proposal.number = this->current_proposal_number;
        // Do not reset the value since it is the original value;
        // reprepare->proposal.value = this->highest_accepted_proposal_value;
        // accept->from_id = this->instance->server->get_id();
        this->instance->deadline_timer.cancel();
        return std::move(resubmit);
    } else {
        return nullptr;
    }
}

void Proposer::accept(std::unique_ptr<Message> accept) {
    // It is not necessary to obtain which nodes sent accept
    // We could simply sent ACCEPT to each of nodes
    // We even do not need to notice whether it sent successfully or not
    accept->from_id = this->instance->server->get_id();

    BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Proposer {} Accept Number {} Value {} {}\n",
                                            accept->sequence,
                                            this->instance->server->get_id(),
                                            accept->proposal.number,
                                            magic_enum::enum_name(accept->proposal.value.operation),
                                            accept->proposal.value.object);

    for(std::uint32_t node_id = 0; node_id < this->instance->server->get_number_of_nodes(); node_id++) {
        // We need to clone the unique_ptr<Message> and just send to all nodes;
        std::unique_ptr<Message> accept_copy = accept->clone();
        std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint = this->instance->server->config->get_addr_by_id(node_id);
        BOOST_LOG_TRIVIAL(trace) << fmt::format("Inst Seq {} : Proposer {} Async Send Accept to Acceptor {}\n",
                                                accept_copy->sequence, this->instance->server->get_id(), node_id);
        this->instance->server->connect->do_send(std::move(accept_copy), std::move(endpoint), do_nothing_handler);
    }

    this->instance->deadline_timer.cancel();
    this->instance->deadline_timer.expires_after(std::chrono::milliseconds(
            this->instance->server->config->after_accept_milliseconds + get_random_number(0, 1000)));
    this->instance->deadline_timer.async_wait(
            [this, old_accept = accept->clone()](const boost::system::error_code& error) mutable {
                if(error) {
                    return ;
                }
                std::unique_ptr<Message> resubmit = std::move(old_accept);
                resubmit->type = MessageType::SUBMIT;
                std::unique_ptr<boost::asio::ip::udp::endpoint> client_endpoint = get_udp_ipv4_endpoint_from_uint64_t(resubmit->proposal.value.client_id);
                BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Server {} Need Resubmit {} {} from Client {}:{} {} since Proposer Accept Timeout\n",
                                                        resubmit->sequence, this->instance->server->get_id(),
                                                        magic_enum::enum_name(resubmit->proposal.value.operation),
                                                        resubmit->proposal.value.object,
                                                        client_endpoint->address().to_string(),
                                                        client_endpoint->port(),
                                                        resubmit->proposal.value.client_once);
                std::unique_ptr<Message> prepare = this->instance->proposer.on_submit(std::move(resubmit));
                if (prepare != nullptr) {
                    instance->proposer.prepare(std::move(prepare));
                }
            });
}

void Proposer::prepare(std::unique_ptr<Message> prepare) {
    prepare->from_id = this->instance->server->get_id();

    BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Proposer {} Prepare Number {}\n",
                                            prepare->sequence,
                                            this->instance->server->get_id(),
                                            prepare->proposal.number);

    for(std::uint32_t node_id = 0; node_id < this->instance->server->get_number_of_nodes(); node_id++) {
        // We need to clone the unique_ptr<Message> and just send to all nodes;
        std::unique_ptr<Message> prepare_copy = prepare->clone();
        std::unique_ptr<boost::asio::ip::udp::endpoint> endpoint = this->instance->server->config->get_addr_by_id(node_id);
        BOOST_LOG_TRIVIAL(trace) << fmt::format("Inst Seq {} : Proposer {} Async Send Prepare to Acceptor {}\n",
                                                prepare_copy->sequence, this->instance->server->get_id(), node_id);
        this->instance->server->connect->do_send(std::move(prepare_copy), std::move(endpoint), do_nothing_handler);
    }

    this->instance->deadline_timer.cancel();
    this->instance->deadline_timer.expires_after(std::chrono::milliseconds(
            this->instance->server->config->after_prepare_milliseconds + get_random_number(0, 1000)));
    this->instance->deadline_timer.async_wait(
            [this, old_prepare = prepare->clone()](const boost::system::error_code& error) mutable {
                if(error) {
                    return ;
                }

                std::unique_ptr<Message> resubmit = std::move(old_prepare);
                resubmit->type = MessageType::SUBMIT;
                std::unique_ptr<boost::asio::ip::udp::endpoint> client_endpoint = get_udp_ipv4_endpoint_from_uint64_t(resubmit->proposal.value.client_id);
                BOOST_LOG_TRIVIAL(debug) << fmt::format("Inst Seq {} : Server {} Need Resubmit {} {} from Client {}:{} {} since Proposal Prepare Timeout\n",
                                                        resubmit->sequence, this->instance->server->get_id(),
                                                        magic_enum::enum_name(resubmit->proposal.value.operation),
                                                        resubmit->proposal.value.object,
                                                        client_endpoint->address().to_string(),
                                                        client_endpoint->port(),
                                                        resubmit->proposal.value.client_once);
                std::unique_ptr<Message> prepare = this->instance->proposer.on_submit(std::move(resubmit));
                if (prepare != nullptr) {
                    instance->proposer.prepare(std::move(prepare));
                }
    });
}
