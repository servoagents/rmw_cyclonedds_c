#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
OUTPUT_DIR="${1:-${LAB_ROOT}/results/phase3-rmw-smoke}"
IMAGE="${PHASE3_IMAGE:-ros2-zephyr-kilted-rmw-c:phase3}"

mkdir -p "${OUTPUT_DIR}"

docker run --rm --network none \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
  -e ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-93}" \
  "${IMAGE}" \
  bash -lc '. /opt/ros/kilted/setup.sh && . /phase3_ws/install/setup.sh && exec /phase3_ws/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/phase3_trace_minimal_rclc' \
  >"${OUTPUT_DIR}/stdout.log" \
  2>"${OUTPUT_DIR}/stderr.log"

grep -q '^PHASE3_TRACE_PASS value=42424242$' "${OUTPUT_DIR}/stdout.log"
if grep -q 'failed to resolve symbol' "${OUTPUT_DIR}/stderr.log"; then
  echo "RMW loader symbol resolution failed; inspect ${OUTPUT_DIR}/stderr.log" >&2
  exit 1
fi
if grep -q 'rcutils_set_error_state' "${OUTPUT_DIR}/stderr.log"; then
  echo "unexpected rcutils error state; inspect ${OUTPUT_DIR}/stderr.log" >&2
  exit 1
fi
if grep -q '^PHASE3_CLEANUP_ERROR' "${OUTPUT_DIR}/stderr.log"; then
  echo "RMW cleanup failed; inspect ${OUTPUT_DIR}/stderr.log" >&2
  exit 1
fi
cat "${OUTPUT_DIR}/stdout.log"
