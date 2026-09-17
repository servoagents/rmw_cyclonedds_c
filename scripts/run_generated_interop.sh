#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "${script_dir}/.." && pwd)"
output_dir="${1:-${repository_root}/results/generated-interop}"
ros_distro="${RMW_CYCLONEDDS_C_ROS_DISTRO:-lyrical}"
image="${RMW_CYCLONEDDS_C_IMAGE:-servoagents/rmw-cyclonedds-c:${ros_distro}}"
domain_id="${ROS_DOMAIN_ID:-95}"
cyclonedds_uri="${RMW_CYCLONEDDS_C_URI:-<CycloneDDS><Domain><General><AllowMulticast>true</AllowMulticast></General></Domain></CycloneDDS>}"
rmw_binary=/workspace/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/generated_interop

mkdir -p "${output_dir}"
: >"${output_dir}/summary.log"

custom_pid=""
ros_pid=""
cleanup() {
  [[ -z "${custom_pid}" ]] || kill "${custom_pid}" 2>/dev/null || true
  [[ -z "${ros_pid}" ]] || kill "${ros_pid}" 2>/dev/null || true
}
trap cleanup EXIT

run_case() {
  local reliability="$1"
  local case_dir="${output_dir}/${reliability}"
  mkdir -p "${case_dir}"

  timeout --signal=KILL 40 docker run --rm --network bridge \
    -e ROS_DOMAIN_ID="${domain_id}" \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
    -e CYCLONEDDS_URI="${cyclonedds_uri}" \
    "${image}" \
    bash -lc ". /opt/ros/${ros_distro}/setup.sh && . /workspace/install/setup.sh && exec ${rmw_binary} sub ${reliability}" \
    >"${case_dir}/ros-to-rmw.rmw.log" 2>&1 &
  custom_pid=$!
  sleep 2
  timeout --signal=KILL 20 docker run --rm --network bridge \
    -e ROS_DOMAIN_ID="${domain_id}" \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
    -e CYCLONEDDS_URI="${cyclonedds_uri}" \
    "${image}" \
    bash -lc ". /opt/ros/${ros_distro}/setup.sh && . /workspace/install/setup.sh && ros2 topic pub \
      --times 10 --rate 5 --wait-matching-subscriptions 0 \
      --qos-reliability ${reliability} --qos-durability volatile \
      --qos-history keep_last --qos-depth 10 \
      /ros_to_generated_rmw cyclonedds_c_test_msgs/msg/NestedFixed \
      '{counter: {data: 31415926}, samples: {values: [21, 22, 23, 24]}}'" \
    >"${case_dir}/ros-to-rmw.ros.log" 2>&1
  wait "${custom_pid}"
  custom_pid=""

  timeout --signal=KILL 30 docker run --rm --network bridge \
    -e ROS_DOMAIN_ID="${domain_id}" \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
    -e CYCLONEDDS_URI="${cyclonedds_uri}" \
    "${image}" \
    bash -lc ". /opt/ros/${ros_distro}/setup.sh && . /workspace/install/setup.sh && ros2 topic echo --once \
      --qos-reliability ${reliability} --qos-durability volatile \
      --qos-history keep_last --qos-depth 10 \
      /generated_rmw_to_ros cyclonedds_c_test_msgs/msg/NestedFixed" \
    >"${case_dir}/rmw-to-ros.ros.log" 2>&1 &
  ros_pid=$!
  sleep 3
  timeout --signal=KILL 35 docker run --rm --network bridge \
    -e ROS_DOMAIN_ID="${domain_id}" \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
    -e CYCLONEDDS_URI="${cyclonedds_uri}" \
    "${image}" \
    bash -lc ". /opt/ros/${ros_distro}/setup.sh && . /workspace/install/setup.sh && exec ${rmw_binary} pub ${reliability}" \
    >"${case_dir}/rmw-to-ros.rmw.log" 2>&1
  wait "${ros_pid}"
  ros_pid=""

  grep -q "^RMW_RECEIVED direction=ros_to_rmw reliability=${reliability} value=31415926 array=21,22,23,24$" \
    "${case_dir}/ros-to-rmw.rmw.log"
  grep -q "^RMW_SENT direction=rmw_to_ros reliability=${reliability} value=27182818 sequence=1$" \
    "${case_dir}/rmw-to-ros.rmw.log"
  grep -q '^  data: 27182818$' "${case_dir}/rmw-to-ros.ros.log"
  grep -q '^  - 31$' "${case_dir}/rmw-to-ros.ros.log"
  if rg -q 'Failed to parse type hash|no type hash' "${case_dir}"; then
    echo "type-hash metadata warning detected for ${reliability}" >&2
    exit 1
  fi

  {
    echo "PASS stock rmw_cyclonedds_cpp -> C RMW reliability=${reliability} value=31415926"
    echo "PASS C RMW -> stock rmw_cyclonedds_cpp reliability=${reliability} value=27182818"
  } | tee -a "${output_dir}/summary.log"
}

run_case best_effort
run_case reliable

{
  echo "domain=${domain_id}"
  echo 'type=cyclonedds_c_test_msgs::msg::dds_::NestedFixed_'
  echo 'actual_qos=verified_by_c_rmw_endpoints'
} | tee -a "${output_dir}/summary.log"
