#pragma once

#include "ISignalPublisher.hpp"

#include <grpcpp/grpcpp.h>
#include <kuksa/val/v2/val.grpc.pb.h>

#include <memory>
#include <string>

namespace sdvgw::kuksa {

/// Connection parameters for the Kuksa Databroker.
struct KuksaConfig {
    std::string endpoint;       ///< host:port, e.g., "localhost:55555"
    bool        tls_enabled{false};
    std::string ca_cert_path;   ///< PEM CA certificate — required when tls_enabled=true
    std::string auth_token;     ///< JWT bearer token — optional, for authorization
};

/// gRPC-based publisher for Eclipse Kuksa Databroker (kuksa.val.v2).
///
/// Implements ISignalPublisher so the upstream pipeline is decoupled from
/// the specific Kuksa API version and transport (see ADR-004).
///
/// TLS + JWT are supported for production deployments (UN R155 compliance).
/// In local Docker development the Databroker runs with --insecure and
/// tls_enabled must be false (see docker-compose.yml).
class KuksaClient final : public ISignalPublisher {
public:
    explicit KuksaClient(KuksaConfig config);

    /// Establish the gRPC channel. Must be called before publish().
    /// Returns true if the channel was created successfully.
    /// (gRPC channels are lazy — actual connectivity is checked on first RPC.)
    bool connect();

    // ISignalPublisher
    bool publish(std::span<const decoder::VssSignal> signals) override;
    bool is_connected() const noexcept override;

private:
    bool publish_one(const decoder::VssSignal& signal);

    KuksaConfig                                  config_;
    std::shared_ptr<grpc::Channel>               channel_;
    std::unique_ptr<::kuksa::val::v2::VAL::Stub>  stub_;
};

} // namespace sdvgw::kuksa
