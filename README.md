# sdv-gateway-cpp

[![CI](https://github.com/danielecostarella/sdv-gateway-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/danielecostarella/sdv-gateway-cpp/actions/workflows/ci.yml)

A minimalist automotive gateway written in C++20 following the **Signal-to-Service** paradigm for Software Defined Vehicle (SDV) platforms.

The gateway reads raw CAN frames from a SocketCAN interface, normalises them against the [COVESA Vehicle Signal Specification (VSS)](https://github.com/COVESA/vehicle_signal_specification), and feeds the resulting typed signals into [Eclipse Kuksa Databroker](https://github.com/eclipse-kuksa/kuksa-databroker) via gRPC — filling a gap in the Eclipse SDV ecosystem where no official C++ CAN feeder exists.

> **Disclaimer:** This project is a personal study and learning exercise exploring modern SDV architectures and the Eclipse automotive open-source ecosystem. It is not intended for production use, does not represent the work or technology of any employer or client, and contains no proprietary or confidential information.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        sdv-gateway-cpp                          │
│                                                                 │
│  ┌──────────────┐   SPSC    ┌──────────────┐   gRPC            │
│  │   Transport  │  Queue    │   Signal     │  (kuksa.val.v2)   │
│  │              │ ────────► │              │ ──────────────►   │
│  │ SocketCAN    │ lock-free │ Decoder      │  Kuksa            │
│  │ Reader       │           │ VSS Mapper   │  Databroker       │
│  └──────────────┘           └──────────────┘                   │
│        ▲                          ▲                             │
│   vcan0 / can0             config/mapping.json                  │
└─────────────────────────────────────────────────────────────────┘
```

**Three decoupled layers**, connected by a lock-free SPSC ring buffer:

| Layer | Directory | Responsibility |
|---|---|---|
| Transport | `src/can/` | Raw CAN frame ingestion via SocketCAN. No parsing, no allocation on the hot path. |
| Signal | `src/decoder/` | Stateless decoding: CAN frame → typed VSS signal. Data-driven JSON mapping. |
| Service | `src/kuksa/` | gRPC client publishing `VssSignal` values to Kuksa Databroker (`kuksa.val.v2`). |

Architecture decisions are documented in [`docs/adr/`](docs/adr/).

---

## Simulated VSS Signals

| CAN ID | Bytes | VSS Path | Type | Factor |
|---|---|---|---|---|
| `0x100` | 0–1 | `Vehicle.Speed` | float | 0.1 km/h |
| `0x100` | 2–3 | `Vehicle.Powertrain.CombustionEngine.Speed` | uint32 | 1 rpm |
| `0x101` | 0 | `Vehicle.Body.Lights.IsHighBeamOn` | bool | — |

Signal mappings are defined in [`config/mapping.json`](config/mapping.json) and loaded at startup — no recompilation required to add or change signals.

---

## Getting Started

### Option A — Docker Compose (Mac, Linux desktop)

Everything runs in containers. `vcan0` is created inside the gateway container's Linux network namespace and shared with the CAN simulator.

**Prerequisites:** Docker Desktop (Mac or Linux) with the Linux VM running.

```bash
git clone https://github.com/danielecostarella/sdv-gateway-cpp.git
cd sdv-gateway-cpp
docker compose up
```

Three services start automatically:
- `kuksa-databroker` — Eclipse Kuksa Databroker on port `55555`
- `gateway` — reads from `vcan0`, publishes VSS signals via gRPC
- `can-simulator` — injects synthetic CAN frames into `vcan0` every 100 ms

To observe live VSS values, install the [Kuksa Python client](https://github.com/eclipse-kuksa/kuksa-python-sdk) and run:

```bash
pip install kuksa-client
kuksa-client --host localhost --port 55555 --insecure
> getValue Vehicle.Speed
> getValue Vehicle.Powertrain.CombustionEngine.Speed
```

### Option B — Raspberry Pi (bare metal)

**Prerequisites:** Raspberry Pi OS (Bookworm, ARM64 or ARMv7), Docker installed for Kuksa Databroker.

```bash
# 1. Install build dependencies
sudo apt update
sudo apt install -y \
  build-essential cmake git \
  libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc \
  nlohmann-json3-dev libspdlog-dev libgtest-dev \
  can-utils ca-certificates

# 2. Clone and build
git clone https://github.com/danielecostarella/sdv-gateway-cpp.git
cd sdv-gateway-cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# 3. Set up virtual CAN interface (once per boot)
./scripts/setup-vcan.sh

# 4. Start Kuksa Databroker
docker run -d -p 55555:55555 \
  ghcr.io/eclipse-kuksa/kuksa-databroker:latest --insecure

# 5. Start CAN simulator (terminal 1)
./build/tools/can-simulator/can-simulator --interface vcan0

# 6. Start gateway (terminal 2)
./build/sdv-gateway --config config/mapping.json --can vcan0
```

**Real CAN hardware** (MCP2515, PiCAN2): replace `vcan0` with `can0` — no code changes required.

---

## Configuration

`config/mapping.json` maps CAN frames to VSS signals:

```json
{
  "version": "1.0",
  "mappings": [
    {
      "can_id": 256,
      "signals": [
        {
          "vss_path": "Vehicle.Speed",
          "type": "float",
          "start_byte": 0,
          "num_bytes": 2,
          "factor": 0.1,
          "offset": 0,
          "unit": "km/h"
        }
      ]
    }
  ]
}
```

| Field | Description |
|---|---|
| `can_id` | Decimal CAN frame identifier |
| `vss_path` | COVESA VSS signal path |
| `type` | `float`, `uint16`, `uint32`, or `bool` |
| `start_byte` | Byte offset within the 8-byte CAN payload |
| `num_bytes` | Signal width in bytes (1–4, little-endian) |
| `factor` | Multiplier applied after extraction |
| `offset` | Additive offset applied after scaling |

The gateway validates the config at startup and exits with a diagnostic on any schema error.

---

## Development

```bash
# Debug build
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# Run all tests
ctest --test-dir build --output-on-failure

# Run a single test suite
ctest --test-dir build -R MappingConfig --output-on-failure

# Static analysis
find src -name "*.cpp" ! -name "placeholder.cpp" \
  | xargs clang-tidy -p build
```

### Gateway options

```
--can      <iface>     CAN interface name    (default: vcan0)
--config   <path>      Mapping config file   (default: config/mapping.json)
--endpoint <host:port> Kuksa Databroker      (default: localhost:55555)
--tls      true|false  Enable TLS            (default: false)
--token    <jwt>       JWT bearer token      (optional)
--verbose              Trace-level logging
```

---

## Eclipse SDV Ecosystem

This gateway integrates with:

| Component | Role |
|---|---|
| [Eclipse Kuksa Databroker](https://github.com/eclipse-kuksa/kuksa-databroker) | Central VSS signal store (gRPC `kuksa.val.v2`) |
| [COVESA VSS](https://github.com/COVESA/vehicle_signal_specification) | Vehicle signal naming and type standard |
| [Eclipse LEDA](https://eclipse-leda.github.io/leda/) | Reference SDV platform for containerised deployment |

[ADR-004](docs/adr/ADR-004-uprotocol-future-communication-layer.md) describes the planned migration path to [Eclipse uProtocol](https://github.com/eclipse-uprotocol) as the inter-domain communication layer.

---

## Security

TLS and JWT authentication are supported for production deployments and required for compliance with **UN R155** (automotive cybersecurity regulation). See [ADR-001](docs/adr/ADR-001-grpc-over-mqtt-primary-transport.md) for details.

Local development uses `--insecure` mode on the Kuksa Databroker (see `docker-compose.yml`).

---

## License

MIT
