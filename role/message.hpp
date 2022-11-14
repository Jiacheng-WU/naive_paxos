//
// Created by Jiacheng Wu on 11/7/22.
//

#ifndef PAXOS_MESSAGE_HPP
#define PAXOS_MESSAGE_HPP

#include <cstdint>

#include <memory>
#include "magic_enum.hpp"
// #include "boost/serialization/serialization.hpp"


enum class MessageType : std::uint32_t {
    UNDEFINED = 0,

    /** For PHASE 1 **/
    PREPARE,
    PROMISE,
    DENIAL, // DENIAL to Prepare

    /** For PHASE 2 **/
    ACCEPT,
    ACCEPTED, // From Acceptor to Learner
    REJECTED, // REJECT to Accept

    /** For Learner **/
    INFORM, // DISTINGUISH learner to other learner, no ack further need
    LEARN, // learner to distinguish learn
    UNDECIDE, // DISTINGUISH learner did not know the current commands fro

//    /** For Heartbeat **/
//    HEARTBEAT,


    /** Client -> Server **/
    SUBMIT, // Client submit Requests to Server
    COMMAND, // Server Internal

    /** Server -> Client **/
    RESPONSE, // Client get Results, Distinguish learner reply
};



struct ProposalValue {
    // do not use enum class otherwise we need to have multiple ::
    enum ProposalOperation: std::uint32_t {
        UNDEFINED = 0,
        ELECT_LEADER,
        LOCK,
        UNLOCK,
        NOOPS,

        // For Clients Reponse
        LOCK_SUCCEED,
        UNLOCK_SUCCEED,
        LOCK_FAILED, // have been previously locked by other clients
        LOCK_AGAIN, // have been previously locked by itself
        UNLOCK_FAILED, // have been not previous locked
        UNLOCK_AGAIN, // unlock on a already unlocked object
    } operation = UNDEFINED;
    std::uint32_t object = 0;
    std::uint64_t client_id = 0;
    std::uint32_t client_once = 0;
};
// Specified proposal for lock service

struct Proposal {
    std::uint32_t number; // n
    ProposalValue value;
};

struct MessageBuffer;

struct Message {
    MessageType type = MessageType::UNDEFINED;

    std::uint32_t sequence = 0; // For Paxos command sequence is mainly for Paxos Instances
    std::uint32_t from_id = 0; // use from id to maintain, as well as the id from client

    Proposal proposal;
    union {
        std::uint32_t additional_field = 0;
        /**
         * For acceptor to identify which prepare_proposal_number
         * In fact, the acceptor can only promise to the corresponding prepare_proposal_number
         * Thus, we need to identify it in on_promise of proposer
         * Ignore if the current proposer already propose a higher number proposal
         **/
        std::uint32_t prepare_proposal_number;
        /**
         * It is not necessary to remember the accept proposal number in accepted message
         * But it may accelerate for the rejected message not avoid next round proposal
         */
        std::uint32_t accept_proposal_number;
    };

    [[nodiscard]] static consteval std::size_t size() {
        return sizeof(Message);
    }

    // Cannot be virtual, otherwise influence the
    [[nodiscard]] std::unique_ptr<Message> clone() const {
        std::unique_ptr<Message> cloned = std::make_unique<Message>();
        // We could customize which attribute should be cloned;
        cloned->type = this->type;
        cloned->sequence = this->sequence;
        cloned->from_id = this->from_id;
        cloned->proposal = this->proposal;
        cloned->additional_field = this->additional_field;
        // cloned->additional_field_2 = this->additional_field_2;
        return std::move(cloned);
    }

// #warning "IGNORE LITTLE ENDIAN OR LARGE ENDIAN"
    void serialize_to(std::uint8_t buffer[size()]) {
        memcpy(buffer, this, size());
    }

    void deserialize_from(std::uint8_t buffer[size()]) {
        memcpy(this, buffer, size());
    }


//    friend class boost::serialization::access;
//    template<class Archive>
//    void serialize(Archive & ar, const unsigned int version)
//    {
//        ar & type;
//        ar & sequence;
//        ar & from_id;
//        ar & proposal;
//        ar & additional_field_1;
//        ar & additional_field_2;
//    }

};

struct MessageBuffer {
    std::uint8_t buffer[Message::size()];
};


struct AcceptorState {
    std::uint32_t sequence;
    std::uint32_t highest_prepare_proposal_number;
    std::uint32_t highest_accepted_proposal_number;
    ProposalValue highest_accepted_proposal_value;
    [[nodiscard]] static consteval std::size_t size() {
        return sizeof(Message);
    }
    void serialize_to(char buffer[size()]) {
        memcpy(buffer, this, size());
    }

    void deserialize_from(char buffer[size()]) {
        memcpy(this, buffer, size());
    }
};

struct LearnerState {
    std::uint32_t sequence;
    std::uint32_t final_informed_proposal_number;
    ProposalValue final_informed_proposal_value;
    [[nodiscard]] static consteval std::size_t size() {
        return sizeof(Message);
    }
    void serialize_to(char buffer[size()]) {
        memcpy(buffer, this, size());
    }

    void deserialize_from(char buffer[size()]) {
        memcpy(this, buffer, size());
    }
};

#endif //PAXOS_MESSAGE_HPP
