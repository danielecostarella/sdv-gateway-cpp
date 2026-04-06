#pragma once

#include <decoder/VssSignal.hpp>
#include <span>

namespace sdvgw::kuksa {

/// Abstract outbound signal publisher.
///
/// The current implementation (KuksaClient) pushes values to Eclipse Kuksa
/// Databroker via gRPC (kuksa.val.v2). A future implementation may publish
/// via Eclipse uProtocol without touching the signal or transport layers
/// (see ADR-004).
class ISignalPublisher {
public:
    virtual ~ISignalPublisher() = default;

    /// Publish a batch of VSS signals.
    /// Returns true if all signals were accepted by the downstream service.
    virtual bool publish(std::span<const decoder::VssSignal> signals) = 0;

    /// Returns true if the publisher has an active connection to its backend.
    virtual bool is_connected() const noexcept = 0;
};

} // namespace sdvgw::kuksa
