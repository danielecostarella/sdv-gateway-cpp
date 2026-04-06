# ---------------------------------------------------------------------------
# Stage 1 — builder
# Targets: linux/amd64, linux/arm64 (Raspberry Pi)
# ---------------------------------------------------------------------------
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    nlohmann-json3-dev \
    libspdlog-dev \
    libgtest-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j$(nproc)

# ---------------------------------------------------------------------------
# Stage 2 — runtime
# Keeps only the gateway binary and config.
# iproute2 + kmod needed to set up vcan0 inside the container.
# Uses -dev packages to avoid version-specific runtime package names
# (e.g. libgrpc++1.51) that change across Debian snapshots.
# ---------------------------------------------------------------------------
FROM debian:bookworm-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libgrpc++-dev \
    libprotobuf-dev \
    libspdlog-dev \
    iproute2 \
    kmod \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /src/build/sdv-gateway              ./sdv-gateway
COPY --from=builder /src/build/tools/can-simulator/can-simulator ./can-simulator
COPY --from=builder /src/docker-entrypoint.sh           ./docker-entrypoint.sh
COPY config/                                             ./config/

RUN chmod +x docker-entrypoint.sh

ENTRYPOINT ["./docker-entrypoint.sh"]
CMD ["./sdv-gateway", "--config", "config/mapping.json", "--can", "vcan0"]
