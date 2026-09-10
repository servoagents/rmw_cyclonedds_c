#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
OUTPUT_DIR="${1:-${LAB_ROOT}/results/phase3-rmw-interop}"
IMAGE="${PHASE3_IMAGE:-ros2-zephyr-kilted-rmw-c:phase3}"
PEER_IMAGE="${PEER_IMAGE:-ros2-zephyr-kilted-peer:phase1}"
ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-94}"
CYCLONEDDS_URI='<CycloneDDS><Domain><General><AllowMulticast>true</AllowMulticast></General></Domain></CycloneDDS>'
RMW_BINARY=/phase3_ws/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/phase3_interop_rclc

mkdir -p "${OUTPUT_DIR}"
: >"${OUTPUT_DIR}/summary.log"

custom_pid=""
ros_pid=""
cleanup() {
  if [[ -n "${custom_pid}" ]]; then
    kill "${custom_pid}" 2>/dev/null || true
  fi
  if [[ -n "${ros_pid}" ]]; then
    kill "${ros_pid}" 2>/dev/null || true
  fi
}
trap cleanup EXIT

timeout --signal=KILL 40 docker run --rm --network bridge \
  -e ROS_DOMAIN_ID="${ROS_DOMAIN_ID}" \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
  -e CYCLONEDDS_URI="${CYCLONEDDS_URI}" \
  "${IMAGE}" \
  bash -lc ". /opt/ros/kilted/setup.sh && . /phase3_ws/install/setup.sh && exec ${RMW_BINARY} sub" \
  >"${OUTPUT_DIR}/ros-to-rmw.rmw.log" 2>&1 &
custom_pid=$!
sleep 2
timeout --signal=KILL 20 docker run --rm --network bridge \
  -e ROS_DOMAIN_ID="${ROS_DOMAIN_ID}" \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
  -e CYCLONEDDS_URI="${CYCLONEDDS_URI}" \
  "${PEER_IMAGE}" \
  ros2 topic pub --times 10 --rate 5 --wait-matching-subscriptions 0 \
  --qos-reliability best_effort --qos-durability volatile \
  --qos-history keep_last --qos-depth 10 \
  /phase3_ros_to_rmw std_msgs/msg/UInt32 '{data: 314159265}' \
  >"${OUTPUT_DIR}/ros-to-rmw.ros.log" 2>&1
wait "${custom_pid}"
custom_pid=""

timeout --signal=KILL 30 docker run --rm --network bridge \
  -e ROS_DOMAIN_ID="${ROS_DOMAIN_ID}" \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
  -e CYCLONEDDS_URI="${CYCLONEDDS_URI}" \
  "${PEER_IMAGE}" \
  ros2 topic echo --once \
  --qos-reliability best_effort --qos-durability volatile \
  --qos-history keep_last --qos-depth 10 \
  /phase3_rmw_to_ros std_msgs/msg/UInt32 \
  >"${OUTPUT_DIR}/rmw-to-ros.ros.log" 2>&1 &
ros_pid=$!
sleep 3
timeout --signal=KILL 35 docker run --rm --network bridge \
  -e ROS_DOMAIN_ID="${ROS_DOMAIN_ID}" \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
  -e CYCLONEDDS_URI="${CYCLONEDDS_URI}" \
  "${IMAGE}" \
  bash -lc ". /opt/ros/kilted/setup.sh && . /phase3_ws/install/setup.sh && exec ${RMW_BINARY} pub" \
  >"${OUTPUT_DIR}/rmw-to-ros.rmw.log" 2>&1
wait "${ros_pid}"
ros_pid=""

grep -q '^RMW_RECEIVED direction=ros_to_rmw value=314159265 expected=314159265$' \
  "${OUTPUT_DIR}/ros-to-rmw.rmw.log"
grep -q 'publishing #1: std_msgs.msg.UInt32(data=314159265)' \
  "${OUTPUT_DIR}/ros-to-rmw.ros.log"
grep -q '^RMW_SENT direction=rmw_to_ros value=271828182 sequence=1$' \
  "${OUTPUT_DIR}/rmw-to-ros.rmw.log"
grep -q '^data: 271828182$' "${OUTPUT_DIR}/rmw-to-ros.ros.log"

{
  echo 'PASS ROS 2 Kilted rmw_cyclonedds_cpp -> rmw_cyclonedds_c value=314159265'
  echo 'PASS rmw_cyclonedds_c -> ROS 2 Kilted rmw_cyclonedds_cpp value=271828182'
  echo "domain=${ROS_DOMAIN_ID}"
  echo 'type=std_msgs::msg::dds_::UInt32_'
} | tee "${OUTPUT_DIR}/summary.log"
