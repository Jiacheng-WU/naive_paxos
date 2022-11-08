//
// Created by Jiacheng Wu on 11/7/22.
//

#ifndef PAXOS_PAXOS_H
#define PAXOS_PAXOS_H

#include "proto/paxos.pb.h"
#include "proto/paxos.grpc.pb.h"

class PaxosExecutor : public PaxosExecutorBase::Service {
  public:
    PaxosExecutor() = default;
    ~PaxosExecutor() override = default;
    ::grpc::Status onPrepare(::grpc::ServerContext *context, const ::Proposal *request, ::Promise *response) override;
    ::grpc::Status onAccept(::grpc::ServerContext *context, const ::Proposal *request, ::Promise *response) override;
    ::grpc::Status onLearn(::grpc::ServerContext *context, const ::Proposal *request, ::EmptyMessage *response) override;

};


#endif //PAXOS_PAXOS_H
