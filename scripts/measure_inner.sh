#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -eo pipefail

# These files exist in the test image.
# shellcheck disable=SC1090
. "/opt/ros/${ROS_DISTRO}/setup.sh"
# shellcheck disable=SC1091
. /workspace/install/setup.sh
set -u

APP=/workspace/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/uint32_interop
RMW_LIBRARY=/workspace/install/rmw_cyclonedds_c/lib/librmw_cyclonedds_c.so
TRACE_APP=/workspace/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/rmw_smoke_test

RMW_IMPLEMENTATION=rmw_cyclonedds_c ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-96}" \
  "${APP}" sub >/tmp/rmw-cyclonedds-c-measure.log 2>&1 &
app_pid=$!
cleanup() {
  kill "${app_pid}" 2>/dev/null || true
  wait "${app_pid}" 2>/dev/null || true
}
trap cleanup EXIT

for _ in $(seq 1 50); do
  if [[ -r "/proc/${app_pid}/status" ]] &&
    [[ "$(awk '/^Threads:/{print $2}' "/proc/${app_pid}/status")" -ge 6 ]]; then
    break
  fi
  sleep 0.1
done

echo 'process_status:'
awk '/^(VmPeak|VmSize|VmHWM|VmRSS|Threads):/{print}' "/proc/${app_pid}/status"
echo 'thread_names:'
for thread_name in /proc/"${app_pid}"/task/*/comm; do
  sed -n '1p' "${thread_name}"
done | sort
echo 'elf_sections:'
size "${RMW_LIBRARY}" "${TRACE_APP}"
