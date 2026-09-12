#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPOSITORY_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUTPUT_DIR="${1:-${REPOSITORY_ROOT}/results/metrics}"
IMAGE="${RMW_CYCLONEDDS_C_IMAGE:-servoagents/rmw-cyclonedds-c:kilted}"

mkdir -p "${OUTPUT_DIR}"

docker run --rm --network none \
  "${IMAGE}" \
  /workspace/src/scripts/measure_inner.sh |
  tee "${OUTPUT_DIR}/metrics.log"
