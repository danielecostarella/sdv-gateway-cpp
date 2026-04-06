# ADR-004: Eclipse uProtocol as Future Communication Layer

**Status:** Proposed (not yet implemented)  
**Date:** 2026-04-06

## Context

The gateway currently publishes VSS signals directly to a local Eclipse Kuksa
Databroker instance via gRPC. This is appropriate for a single-ECU deployment
where the gateway and the Databroker are co-located.

In a distributed SDV architecture — multiple domains, multiple ECUs, a
high-performance compute (HPC) node, and a cloud backend — services need a
common communication fabric that provides:

- **Service discovery:** how does a consumer find a provider?
- **Addressing:** how are messages routed across ECU boundaries?
- **Transport independence:** the same application logic should work over
  different transports (CAN, Ethernet, cloud).

**Eclipse uProtocol** (supported by GM, Mercedes-Benz, Bosch, and others;
incubated at Eclipse Foundation in 2023) is designed to solve exactly this
problem. It defines a layered communication model:

- **uP-L1:** transport binding (Zenoh, MQTT, SOME/IP, Android Binder, …)
- **uP-L2:** message addressing and routing
- **uP-L3:** service contract (RPC, pub/sub, notifications)

## Decision

The current gateway architecture is designed to be **uProtocol-compatible**
in a future revision, without requiring uProtocol today.

Concretely:
- The `ICANReader` abstraction already decouples the transport from the
  signal processing logic.
- The `KuksaClient` in `src/kuksa/` will be implemented behind an interface
  (`ISignalPublisher`) that can be swapped for a uProtocol publisher without
  touching the signal or transport layers.
- No uProtocol dependency is introduced now; the abstraction cost is minimal
  (one additional interface).

When uProtocol adoption matures and a stable C++ SDK is available, the
migration path is: implement `ISignalPublisher` over uProtocol, update the
wiring in `main.cpp`, and update the deployment config. No layer refactoring
is required.

## Rationale

- **Forward compatibility:** uProtocol is gaining traction as the
  inter-domain communication standard in next-generation SDV platforms.
  Designing for it now avoids a costly refactor later.
- **Low cost today:** the `ISignalPublisher` interface adds one header and
  one indirection. The benefit-to-cost ratio is high.
- **Avoid premature adoption:** the uProtocol C++ SDK is still maturing.
  Depending on it now would introduce an unstable dependency with no
  production benefit.

## Consequences

- `src/kuksa/KuksaClient` will implement `ISignalPublisher`, not be
  instantiated directly in `main.cpp`.
- A future `UProtocolPublisher` implementation can be added without touching
  existing layers.
- This ADR should be revisited when the Eclipse uProtocol C++ SDK reaches
  a stable release.

## References

- https://github.com/eclipse-uprotocol
- https://github.com/eclipse-uprotocol/up-spec
