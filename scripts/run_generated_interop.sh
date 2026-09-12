#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPOSITORY_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUTPUT_DIR="${1:-${REPOSITORY_ROOT}/results/generated-interop}"
IMAGE="${RMW_CYCLONEDDS_C_IMAGE:-servoagents/rmw-cyclonedds-c:kilted}"
ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-95}"
CYCLONEDDS_URI='<CycloneDDS><Domain><General><AllowMulticast>true</AllowMulticast></General></Domain></CycloneDDS>'
RMW_BINARY=/workspace/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/generated_interop

mkdir -p "${OUTPUT_DIR}"
: >"${OUTPUT_DIR}/summary.log"

custom_pid=""
ros_pid=""
cleanup() {
  [[ -z "${custom_pid}" ]] || kill "${custom_pid}" 2>/dev/null || true
  [[ -z "${ros_pid}" ]] || kill "${ros_pid}" 2>/dev/null || true
}
trap cleanup EXIT

timeout --signal=KILL 40 docker run --rm --network bridge \
  -e ROS_DOMAIN_ID="${ROS_DOMAIN_ID}" \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
  -e CYCLONEDDS_URI="${CYCLONEDDS_URI}" \
  "${IMAGE}" \
  bash -lc ". /opt/ros/kilted/setup.sh && . /workspace/install/setup.sh && exec ${RMW_BINARY} sub" \
  >"${OUTPUT_DIR}/ros-to-rmw.rmw.log" 2>&1 &
custom_pid=$!
sleep 2
timeout --signal=KILL 20 docker run --rm --network bridge \
  -e ROS_DOMAIN_ID="${ROS_DOMAIN_ID}" \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
  -e CYCLONEDDS_URI="${CYCLONEDDS_URI}" \
  "${IMAGE}" \
  bash -lc ". /opt/ros/kilted/setup.sh && . /workspace/install/setup.sh && ros2 topic pub \
    --times 10 --rate 5 --wait-matching-subscriptions 0 \
    --qos-reliability best_effort --qos-durability volatile \
    --qos-history keep_last --qos-depth 10 \
    /ros_to_generated_rmw cyclonedds_c_test_msgs/msg/NestedFixed \
    '{counter: {data: 31415926}, samples: {values: [21, 22, 23, 24]}}'" \
  >"${OUTPUT_DIR}/ros-to-rmw.ros.log" 2>&1
wait "${custom_pid}"
custom_pid=""

timeout --signal=KILL 30 docker run --rm --network bridge \
  -e ROS_DOMAIN_ID="${ROS_DOMAIN_ID}" \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
  -e CYCLONEDDS_URI="${CYCLONEDDS_URI}" \
  "${IMAGE}" \
  bash -lc ". /opt/ros/kilted/setup.sh && . /workspace/install/setup.sh && ros2 topic echo --once \
    --qos-reliability best_effort --qos-durability volatile \
    --qos-history keep_last --qos-depth 10 \
    /generated_rmw_to_ros cyclonedds_c_test_msgs/msg/NestedFixed" \
  >"${OUTPUT_DIR}/rmw-to-ros.ros.log" 2>&1 &
ros_pid=$!
sleep 3
timeout --signal=KILL 35 docker run --rm --network bridge \
  -e ROS_DOMAIN_ID="${ROS_DOMAIN_ID}" \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
  -e CYCLONEDDS_URI="${CYCLONEDDS_URI}" \
  "${IMAGE}" \
  bash -lc ". /opt/ros/kilted/setup.sh && . /workspace/install/setup.sh && exec ${RMW_BINARY} pub" \
  >"${OUTPUT_DIR}/rmw-to-ros.rmw.log" 2>&1
wait "${ros_pid}"
ros_pid=""

grep -q '^RMW_RECEIVED direction=ros_to_rmw value=31415926 array=21,22,23,24$' \
  "${OUTPUT_DIR}/ros-to-rmw.rmw.log"
grep -q '^RMW_SENT direction=rmw_to_ros value=27182818 sequence=1$' \
  "${OUTPUT_DIR}/rmw-to-ros.rmw.log"
grep -q '^  data: 27182818$' "${OUTPUT_DIR}/rmw-to-ros.ros.log"
grep -q '^  - 31$' "${OUTPUT_DIR}/rmw-to-ros.ros.log"
if rg -q 'Failed to parse type hash|no type hash' "${OUTPUT_DIR}"; then
  echo 'type-hash metadata warning detected' >&2
  exit 1
fi

{
  echo 'PASS stock rmw_cyclonedds_cpp -> generated C RMW nested fixed type value=31415926'
  echo 'PASS generated C RMW -> stock rmw_cyclonedds_cpp nested fixed type value=27182818'
  echo 'PASS endpoint USER_DATA carries canonical ROS type hash'
  echo "domain=${ROS_DOMAIN_ID}"
  echo 'type=cyclonedds_c_test_msgs::msg::dds_::NestedFixed_'
} | tee "${OUTPUT_DIR}/summary.log"
