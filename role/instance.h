//
// Created by Jiacheng Wu on 11/7/22.
//

#ifndef PAXOS_INSTANCE_H
#define PAXOS_INSTANCE_H

#include <cstdint>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include "proposer.h"
#include "acceptor.h"
#include "learner.h"

class PaxosServer;


struct Instance {
    std::uint32_t seq;
    PaxosServer* server;
    Proposer proposer;
    Acceptor acceptor;
    Learner learner;
    boost::asio::steady_timer deadline_timer;
    Instance(std::uint32_t seq, PaxosServer* server);
};

class Instances {
  private:
    PaxosServer* server;
  public:

    Instances(PaxosServer* server): server(server) {}
    std::map<std::uint32_t, std::unique_ptr<Instance>> instances;

    std::shared_mutex mu;

    Instance* get_instance(std::uint32_t instance_seq);

};


#endif //PAXOS_INSTANCE_H
