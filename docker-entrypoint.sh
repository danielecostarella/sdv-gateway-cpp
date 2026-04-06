#!/bin/sh
# Entrypoint for the gateway container.
# Sets up the virtual CAN interface (vcan0) inside the container
# when CAP_NET_ADMIN and CAP_SYS_MODULE are granted via docker-compose.
set -e

modprobe vcan 2>/dev/null || true
ip link add vcan0 type vcan 2>/dev/null || true
ip link set vcan0 up       2>/dev/null || true

exec "$@"
