//
// Created by Jiacheng Wu on 11/7/22.
//

#ifndef PAXOS_MESSAGE_H
#define PAXOS_MESSAGE_H

#include <cstdint>


#include "boost/serialization/serialization.hpp"


enum class MessageType : std::uint32_t {
    PREPARE,
    PREPARE_REPLY,
    ACCEPT,
    ACCEPT_REPLY,
    LEARN,
    SUBMIT, // Client submit Requests
    SUBMIT_REPLY // Client get Results
};



struct ProposalValue {
    enum ProposalOperation: std::uint32_t {
        ELECT_LEADER,
        LOCK,
        UNLOCK
    } operation;
    std::uint32_t object;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & operation;
        ar & object;
    }
};
// Specified proposal for lock service

struct Proposal {
    std::uint32_t number; // n
    ProposalValue value;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & number;
        ar & value;
    }
};

struct Message {
    uint32_t instance;
    uint32_t from_id; // std::limits<uint32_t>::max() represents the clients
    MessageType type;
    Proposal proposal;

    [[nodiscard]] static consteval std::size_t size() {
        return sizeof(Message);
    }

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & instance;
        ar & from_id;
        ar & type;
        ar & proposal;
    }
};



#endif //PAXOS_MESSAGE_H
