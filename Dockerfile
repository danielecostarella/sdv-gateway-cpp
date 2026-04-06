# ---------------------------------------------------------------------------
# Stage 1 — builder
# Uses ubuntu:22.04 to match the GitHub Actions runner environment
# (same GCC version, same package versions → consistent with CI).
# Targets: linux/amd64, linux/arm64 (Raspberry Pi)
# ---------------------------------------------------------------------------
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
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
# iproute2 + kmod needed to set up vcan0 inside the container.
# ---------------------------------------------------------------------------
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libgrpc++-dev \
    libprotobuf-dev \
    libspdlog-dev \
    iproute2 \
    kmod \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /src/build/sdv-gateway     ./sdv-gateway
COPY --from=builder /src/build/can-simulator   ./can-simulator
COPY --from=builder /src/docker-entrypoint.sh                       ./docker-entrypoint.sh
COPY config/                                                         ./config/

RUN chmod +x docker-entrypoint.sh

ENTRYPOINT ["./docker-entrypoint.sh"]
CMD ["./sdv-gateway", "--config", "config/mapping.json", "--can", "vcan0"]
