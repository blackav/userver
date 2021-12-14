#include <tests/service_fixture_test.hpp>

#include <fmt/format.h>
#include <grpcpp/security/credentials.h>

#include <userver/ugrpc/client/channels.hpp>

USERVER_NAMESPACE_BEGIN

GrpcServiceFixture::GrpcServiceFixture() { server_.AddListeningPort(0); }

GrpcServiceFixture::~GrpcServiceFixture() = default;

void GrpcServiceFixture::StartServer() {
  server_.Start();
  channel_ =
      ugrpc::client::MakeChannel(engine::current_task::GetTaskProcessor(),
                                 ::grpc::InsecureChannelCredentials(),
                                 fmt::format("[::1]:{}", server_.GetPort()));
}

void GrpcServiceFixture::StopServer() noexcept {
  channel_.reset();
  server_.Stop();
}

std::shared_ptr<::grpc::Channel> GrpcServiceFixture::GetChannel() {
  UASSERT(channel_);
  return channel_;
}

::grpc::CompletionQueue& GrpcServiceFixture::GetQueue() {
  return server_.GetCompletionQueue();
}

USERVER_NAMESPACE_END