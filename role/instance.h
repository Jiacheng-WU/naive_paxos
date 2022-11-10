//
// Created by Jiacheng Wu on 11/7/22.
//

#ifndef PAXOS_INSTANCE_H
#define PAXOS_INSTANCE_H

#include <cstdint>
#include <map>
#include <mutex>
#include <shared_mutex>
#include "proposer.h"
#include "acceptor.h"
#include "learner.h"

struct Instance {
    std::shared_mutex mutex;
    std::uint32_t seq;
    Proposer proposer;
    Acceptor acceptor;
    Learner learner;
    Instance(std::uint32_t seq): seq(seq) {};
};

class Instances {

  public:

    std::map<std::uint32_t, std::unique_ptr<Instance>> instances;

    std::shared_mutex mu;

    Instance* get_instance(std::uint32_t instance_seq) {
        Instance *instance = nullptr;
        std::map<std::uint32_t, std::unique_ptr<Instance>>::iterator it;
        {
            std::shared_lock<std::shared_mutex> lock(mu);
            it = instances.find(instance_seq);
            if (it != instances.end()) {
                instance = it->second.get();
                return instance;
            }
        }
        {
            std::unique_lock<std::shared_mutex> lock(mu);
            if (it == instances.end()) {
                instances[instance_seq] = std::make_unique<Instance>(instance_seq);
                instance = instances[instance_seq].get();
                return instance;
            }
        }
        return instance; // Also won't reach here
    }

};


#endif //PAXOS_INSTANCE_H
