# ADR-001: gRPC as Primary Outbound Transport to Kuksa Databroker

**Status:** Accepted  
**Date:** 2026-04-06

## Context

The gateway must forward normalized VSS signals to a signal store that vehicle
applications can subscribe to. Two realistic options exist in the Eclipse SDV
ecosystem:

1. **gRPC** — native interface of Eclipse Kuksa Databroker (`kuksa.val.v2`).
   Strongly typed, bidirectional streaming, HTTP/2 transport, protobuf
   serialization.
2. **MQTT** — lightweight pub/sub. Widely used for cloud integration and
   edge-to-cloud bridging. Kuksa does not expose a native MQTT interface;
   MQTT would require a separate broker (e.g., Eclipse Mosquitto) and a
   custom topic/payload convention.

## Decision

Use **gRPC with the `kuksa.val.v2` API** as the primary outbound transport.

MQTT is explicitly out of scope for the gateway core. If cloud or
event-streaming integration is needed in the future, an independent MQTT
bridge component can subscribe to Kuksa Databroker and republish — keeping
that responsibility outside this process.

## Rationale

- **Native fit:** Kuksa Databroker is the de-facto VSS signal store in the
  Eclipse SDV ecosystem. Its gRPC API is the intended feeder interface.
- **Type safety:** protobuf schema enforces signal types at the protocol
  level, avoiding silent type coercions.
- **Bidirectionality:** gRPC streaming allows future actuation (Databroker →
  gateway → CAN) without a second protocol.
- **Separation of concerns:** keeping MQTT out of the gateway avoids coupling
  the feeder to cloud topology decisions.

## Consequences

- The gateway depends on `libgrpc++` and `libprotobuf` at runtime.
- Proto files must be kept in sync with the upstream Kuksa Databroker release
  (automated via `scripts/fetch-proto.sh`).
- Local development requires a running Kuksa Databroker instance (provided
  via `docker compose up`).
- TLS is disabled by default in the local Docker stack (`--insecure` flag on
  the Databroker); TLS + JWT must be enabled in any non-development deployment
  (see UN R155 compliance requirements).
