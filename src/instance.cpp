//
// Created by Jiacheng Wu on 11/7/22.
//

#include "instance.hpp"
#include "server.hpp"

Instance::Instance(std::uint32_t seq, PaxosServer *server) :
        seq(seq), server(server), proposer(this), acceptor(this), learner(this),
        deadline_timer(server->socket.get_executor()) {

}

Instance *Instances::get_instance(std::uint32_t instance_seq) {
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
            instances[instance_seq] = std::make_unique<Instance>(instance_seq, server);
            instance = instances[instance_seq].get();
            return instance;
        }
    }
    return instance; // Also won't reach here
}