#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

docker build \
  --provenance=false \
  --file "${SCRIPT_DIR}/Dockerfile.phase3" \
  --tag ros2-zephyr-kilted-rmw-c:phase3 \
  "${SCRIPT_DIR}"
