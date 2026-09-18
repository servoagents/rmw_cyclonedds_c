#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "${script_dir}/.." && pwd)"
output_dir="${1:-${repository_root}/results/transient-local}"
ros_distro="${RMW_CYCLONEDDS_C_ROS_DISTRO:-kilted}"
image="${RMW_CYCLONEDDS_C_IMAGE:-servoagents/rmw-cyclonedds-c:${ros_distro}}"
base_domain_id="${ROS_DOMAIN_ID:-97}"
cyclonedds_uri="${RMW_CYCLONEDDS_C_URI:-<CycloneDDS><Domain><General><AllowMulticast>true</AllowMulticast></General></Domain></CycloneDDS>}"
rmw_binary=/workspace/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/transient_local_test
peer=/workspace/src/scripts/transient_local_peer.py

mkdir -p "${output_dir}"
: >"${output_dir}/summary.log"

first_pid=""
cleanup() {
  [[ -z "${first_pid}" ]] || kill "${first_pid}" 2>/dev/null || true
}
trap cleanup EXIT

wait_for_marker() {
  local marker="$1"
  local log="$2"
  for _ in {1..300}; do
    grep -q "${marker}" "${log}" && return 0
    sleep 0.1
  done
  return 1
}

run_depth() {
  local depth="$1"
  local domain_id=$((base_domain_id + depth))
  local case_dir="${output_dir}/depth-${depth}"
  mkdir -p "${case_dir}"

  timeout --signal=KILL 45 docker run --rm --network bridge \
    -e ROS_DOMAIN_ID="${domain_id}" \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
    -e CYCLONEDDS_URI="${cyclonedds_uri}" \
    "${image}" \
    bash -lc ". /opt/ros/${ros_distro}/setup.sh && . /workspace/install/setup.sh && exec ${rmw_binary} pub ${depth}" \
    >"${case_dir}/c-publisher.log" 2>&1 &
  first_pid=$!
  wait_for_marker "^RMW_TRANSIENT_LOCAL_READY role=pub depth=${depth}" \
    "${case_dir}/c-publisher.log"
  timeout --signal=KILL 40 docker run --rm --network bridge \
    -e ROS_DOMAIN_ID="${domain_id}" \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
    -e CYCLONEDDS_URI="${cyclonedds_uri}" \
    "${image}" \
    bash -lc ". /opt/ros/${ros_distro}/setup.sh && . /workspace/install/setup.sh && exec python3 ${peer} sub --depth ${depth}" \
    >"${case_dir}/stock-subscriber.log" 2>&1
  wait "${first_pid}"
  first_pid=""

  timeout --signal=KILL 45 docker run --rm --network bridge \
    -e ROS_DOMAIN_ID="${domain_id}" \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
    -e CYCLONEDDS_URI="${cyclonedds_uri}" \
    "${image}" \
    bash -lc ". /opt/ros/${ros_distro}/setup.sh && . /workspace/install/setup.sh && exec python3 ${peer} pub --depth ${depth}" \
    >"${case_dir}/stock-publisher.log" 2>&1 &
  first_pid=$!
  wait_for_marker "^STOCK_TRANSIENT_LOCAL_READY role=pub depth=${depth}" \
    "${case_dir}/stock-publisher.log"
  timeout --signal=KILL 40 docker run --rm --network bridge \
    -e ROS_DOMAIN_ID="${domain_id}" \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
    -e CYCLONEDDS_URI="${cyclonedds_uri}" \
    "${image}" \
    bash -lc ". /opt/ros/${ros_distro}/setup.sh && . /workspace/install/setup.sh && exec ${rmw_binary} sub ${depth}" \
    >"${case_dir}/c-subscriber.log" 2>&1
  wait "${first_pid}"
  first_pid=""

  grep -q "^RMW_TRANSIENT_LOCAL_PASS direction=c_to_stock depth=${depth} " \
    "${case_dir}/c-publisher.log"
  grep -q "^STOCK_TRANSIENT_LOCAL_PASS direction=c_to_stock depth=${depth} " \
    "${case_dir}/stock-subscriber.log"
  grep -q "^STOCK_TRANSIENT_LOCAL_PASS direction=stock_to_c depth=${depth} " \
    "${case_dir}/stock-publisher.log"
  grep -q "^RMW_TRANSIENT_LOCAL_PASS direction=stock_to_c depth=${depth} " \
    "${case_dir}/c-subscriber.log"

  {
    echo "PASS C RMW publisher -> stock rmw_cyclonedds_cpp late subscriber depth=${depth}"
    echo "PASS stock rmw_cyclonedds_cpp publisher -> C RMW late subscriber depth=${depth}"
  } | tee -a "${output_dir}/summary.log"
}

run_depth 1
run_depth 3

echo "profile=reliable,transient_local,keep_last" | tee -a "${output_dir}/summary.log"
