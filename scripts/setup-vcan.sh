#!/usr/bin/env bash
# Sets up a virtual CAN interface (vcan0) on the host for bare-metal
# development on Linux / Raspberry Pi.
# Run once per boot, or add to /etc/rc.local for persistence.
set -euo pipefail

sudo modprobe vcan
sudo ip link add vcan0 type vcan 2>/dev/null || true
sudo ip link set vcan0 up

echo "vcan0 is up"
ip link show vcan0
