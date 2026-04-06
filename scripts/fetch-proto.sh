#!/usr/bin/env bash
# Downloads the kuksa.val.v2 proto file from the official Eclipse Kuksa
# Databroker repository. CMake runs this automatically at configure time
# if the file is missing, but this script can be run manually if needed.
set -euo pipefail

REPO="https://raw.githubusercontent.com/eclipse-kuksa/kuksa-databroker/main"
PROTO_DIR="$(dirname "$0")/../proto/kuksa/val/v2"

mkdir -p "${PROTO_DIR}"
curl -fsSL "${REPO}/proto/kuksa/val/v2/val.proto" -o "${PROTO_DIR}/val.proto"

echo "Downloaded proto/kuksa/val/v2/val.proto"
