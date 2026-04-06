#!/usr/bin/env bash
# Downloads the kuksa.val.v2 proto files from the official Eclipse Kuksa
# Databroker repository. CMake fetches them automatically at configure time
# if missing, but this script can be run manually if needed.
#
# val.proto  — VAL service definition
# types.proto — Datapoint, Value, SignalID and other message types
#               (imported by val.proto — both files are required)
set -euo pipefail

REPO="https://raw.githubusercontent.com/eclipse-kuksa/kuksa-databroker/main"
PROTO_DIR="$(dirname "$0")/../proto/kuksa/val/v2"

mkdir -p "${PROTO_DIR}"

for PROTO in types.proto val.proto; do
    curl -fsSL "${REPO}/proto/kuksa/val/v2/${PROTO}" -o "${PROTO_DIR}/${PROTO}"
    echo "Downloaded proto/kuksa/val/v2/${PROTO}"
done
