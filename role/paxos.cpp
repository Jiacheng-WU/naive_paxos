//
// Created by Jiacheng Wu on 11/7/22.
//

#include "paxos.h"

::grpc::Status
PaxosExecutor::onPrepare(::grpc::ServerContext *context, const ::Proposal *request, ::Promise *response) {
    return Service::onPrepare(context, request, response);
}

::grpc::Status PaxosExecutor::onAccept(::grpc::ServerContext *context, const ::Proposal *request, ::Promise *response) {
    return Service::onAccept(context, request, response);
}

::grpc::Status
PaxosExecutor::onLearn(::grpc::ServerContext *context, const ::Proposal *request, ::EmptyMessage *response) {
    return Service::onLearn(context, request, response);
}
