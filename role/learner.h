//
// Created by Jiacheng Wu on 10/31/22.
//

#ifndef PAXOS_LEARNER_H
#define PAXOS_LEARNER_H

#include "message.h"

class Learner {
  public:
    // For distinguished learner
    std::unique_ptr<Message> on_accepted(std::unique_ptr<Message> accepted) {

        return std::move(accepted);
    }

    std::unique_ptr<Message> on_inform(std::unique_ptr<Message> inform) {
        return std::move(inform);
    }

    std::unique_ptr<Message> on_learn(std::unique_ptr<Message> learn) {
        return std::move(learn);
    }
};


#endif //PAXOS_LEARNER_H
