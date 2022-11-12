//
// Created by Jiacheng Wu on 11/7/22.
//

#ifndef PAXOS_MESSAGE_H
#define PAXOS_MESSAGE_H

#include <cstdint>

#include <memory>

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



    /** Client -> Server **/
    SUBMIT, // Client submit Requests to Server
    REDIRECT, // Non-Leader Server REDIRECT TO Leader Server

    /** Server -> Client **/
    RESPONSE, // Client get Results, Distinguish learner reply
    REDIRECTED,
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
        LOCK_FAILED, // have been previously locked
        UNLOCK_FAILED // have been not previous locked
    } operation = UNDEFINED;
    std::uint32_t object = 0;
};
// Specified proposal for lock service

struct Proposal {
    std::uint32_t number; // n
    ProposalValue value;
};

struct MessageBuffer;

struct Message {
    MessageType type = MessageType::UNDEFINED;

    uint32_t sequence = 0; // For Paxos command sequence is mainly for Paxos Instances

    union {
        uint32_t from_id = 0; // use from id to maintain, as well as the id from client
        /**
         * In fact, we may not necessary to maintain client id
         * But have this one is better to maintain only-once semantics
         * Though required clients have different id
         * Meanwhile, the client endpoints can also be attached with client_id
         * Though the client endpoints could also be attached with sequence
         * We should maintain the client endpoints in Server with map
         * In real, the client_id could at least be uint64_t with MAC and PORT
         **/
        uint32_t client_id;
        uint32_t redirect_id; // For Redirect Message
    };
    Proposal proposal;
    union {
        uint32_t additional_field_1 = 0;
        /**
         * For acceptor to identify which prepare_proposal_number
         * In fact, the acceptor can only promise to the corresponding prepare_proposal_number
         * Thus, we need to identify it in on_promise of proposer
         * Ignore if the current proposer already propose a higher number proposal
         **/
        uint32_t prepare_proposal_number;
        /**
         * It is not necessary to remember the accept proposal number in accepted message
         * But it may accelerate for the rejected message not avoid next round proposal
         */
        uint32_t accept_proposal_number;
        uint32_t client_once; // For Client -> Server
    };
    union {
        uint32_t additional_field_2 = 0;
    };

    [[nodiscard]] static consteval std::size_t size() {
        return sizeof(Message);
    }

    // Cannot be virtual, otherwise influence the
    std::unique_ptr<Message> clone() const {
        std::unique_ptr<Message> cloned = std::make_unique<Message>();
        // We could customize which attribute should be cloned;
        cloned->type = this->type;
        cloned->sequence = this->sequence;
        cloned->from_id = this->from_id;
        cloned->proposal = this->proposal;
        cloned->additional_field_1 = this->additional_field_1;
        cloned->additional_field_2 = this->additional_field_2;
        return std::move(cloned);
    }

// #warning "IGNORE LITTLE ENDIAN OR LARGE ENDIAN"
    void serialize_to(char buffer[size()]) {
        memcpy(buffer, this, size());
    }

    void deserialize_from(char buffer[size()]) {
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
    char buffer[Message::size()];
};


#endif //PAXOS_MESSAGE_H
