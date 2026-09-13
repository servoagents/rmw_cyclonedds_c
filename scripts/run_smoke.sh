#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPOSITORY_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUTPUT_DIR="${1:-${REPOSITORY_ROOT}/results/smoke}"
ROS_DISTRO="${RMW_CYCLONEDDS_C_ROS_DISTRO:-lyrical}"
IMAGE="${RMW_CYCLONEDDS_C_IMAGE:-servoagents/rmw-cyclonedds-c:${ROS_DISTRO}}"

mkdir -p "${OUTPUT_DIR}"

docker run --rm --network none \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
  -e ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-93}" \
  "${IMAGE}" \
  bash -lc ". /opt/ros/${ROS_DISTRO}/setup.sh && . /workspace/install/setup.sh && exec /workspace/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/rmw_smoke_test" \
  >"${OUTPUT_DIR}/stdout.log" \
  2>"${OUTPUT_DIR}/stderr.log"

grep -q '^RMW_CYCLONEDDS_C_SMOKE_PASS value=42424242$' "${OUTPUT_DIR}/stdout.log"
if grep -q 'failed to resolve symbol' "${OUTPUT_DIR}/stderr.log"; then
  echo "RMW loader symbol resolution failed; inspect ${OUTPUT_DIR}/stderr.log" >&2
  exit 1
fi
if grep -q 'rcutils_set_error_state' "${OUTPUT_DIR}/stderr.log"; then
  echo "unexpected rcutils error state; inspect ${OUTPUT_DIR}/stderr.log" >&2
  exit 1
fi
if grep -q '^RMW_CYCLONEDDS_C_CLEANUP_ERROR' "${OUTPUT_DIR}/stderr.log"; then
  echo "RMW cleanup failed; inspect ${OUTPUT_DIR}/stderr.log" >&2
  exit 1
fi
cat "${OUTPUT_DIR}/stdout.log"
