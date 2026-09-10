#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
OUTPUT_DIR="${1:-${LAB_ROOT}/results/phase3-rmw-metrics}"
IMAGE="${PHASE3_IMAGE:-ros2-zephyr-kilted-rmw-c:phase3}"

mkdir -p "${OUTPUT_DIR}"

docker run --rm --network none \
  "${IMAGE}" \
  /phase3_ws/src/rmw_cyclonedds_c/measure_inner.sh \
  | tee "${OUTPUT_DIR}/metrics.log"
