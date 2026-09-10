#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
OUTPUT_DIR="${1:-${LAB_ROOT}/results/phase3-reference-trace}"
IMAGE="${PHASE3_IMAGE:-ros2-zephyr-kilted-rmw-c:phase3}"

mkdir -p "${OUTPUT_DIR}"

docker run --rm --network none "${IMAGE}" \
  nm -D --defined-only /opt/ros/kilted/lib/librmw_implementation.so \
  | awk '$3 ~ /^rmw_/ {print $3}' \
  | sort -u >"${OUTPUT_DIR}/loader-symbols.txt"

docker run --rm --network none "${IMAGE}" \
  nm -D --defined-only /phase3_ws/install/rmw_cyclonedds_c/lib/librmw_cyclonedds_c.so \
  | awk '$3 ~ /^rmw_/ {print $3}' \
  | sort -u >"${OUTPUT_DIR}/backend-symbols.txt"

comm -23 "${OUTPUT_DIR}/loader-symbols.txt" "${OUTPUT_DIR}/backend-symbols.txt" \
  >"${OUTPUT_DIR}/missing-loader-symbols.txt"
if [[ -s "${OUTPUT_DIR}/missing-loader-symbols.txt" ]]; then
  echo "backend does not export the complete Kilted loader ABI:" >&2
  cat "${OUTPUT_DIR}/missing-loader-symbols.txt" >&2
  exit 1
fi

docker run --rm --network none \
  --cap-drop ALL \
  --cap-add SYS_PTRACE \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
  -e ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-92}" \
  "${IMAGE}" \
  bash -lc '. /opt/ros/kilted/setup.sh && . /phase3_ws/install/setup.sh && exec ltrace -f -e "rmw_*" /phase3_ws/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/phase3_trace_minimal_rclc' \
  >"${OUTPUT_DIR}/stdout.log" \
  2>"${OUTPUT_DIR}/runtime-calls.log"

grep -o 'rmw_[a-zA-Z0-9_]*(' "${OUTPUT_DIR}/runtime-calls.log" \
  | sed 's/($//' \
  | sort -u >"${OUTPUT_DIR}/runtime-symbols.txt"

grep -q '^PHASE3_TRACE_PASS value=42424242$' "${OUTPUT_DIR}/stdout.log"
cat "${OUTPUT_DIR}/runtime-symbols.txt"
