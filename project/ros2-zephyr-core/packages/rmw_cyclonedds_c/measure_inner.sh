#!/usr/bin/env bash
set -eo pipefail

. /opt/ros/kilted/setup.sh
. /phase3_ws/install/setup.sh
set -u

APP=/phase3_ws/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/phase3_interop_rclc
RMW_LIBRARY=/phase3_ws/install/rmw_cyclonedds_c/lib/librmw_cyclonedds_c.so
TRACE_APP=/phase3_ws/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/phase3_trace_minimal_rclc

RMW_IMPLEMENTATION=rmw_cyclonedds_c ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-96}" \
  "${APP}" sub >/tmp/phase3-measure-app.log 2>&1 &
app_pid=$!
cleanup() {
  kill "${app_pid}" 2>/dev/null || true
  wait "${app_pid}" 2>/dev/null || true
}
trap cleanup EXIT

for _ in $(seq 1 50); do
  if [[ -r "/proc/${app_pid}/status" ]] &&
    [[ "$(awk '/^Threads:/{print $2}' "/proc/${app_pid}/status")" -ge 6 ]]
  then
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
