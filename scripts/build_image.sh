#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ros_distro="${RMW_CYCLONEDDS_C_ROS_DISTRO:-lyrical}"
image="${RMW_CYCLONEDDS_C_IMAGE:-servoagents/rmw-cyclonedds-c:${ros_distro}}"

case "${ros_distro}" in
  lyrical)
    default_base_image="ros:lyrical-ros-base-resolute@sha256:dbb2a254523ee3c40ec9fc07956bc1042253c7beb3b6dad4a4585d85c99e9716"
    ;;
  kilted)
    default_base_image="ros:kilted-ros-base-noble@sha256:0030f32dc8a71ef8401c89470db6003c779f036f532d40195790f58f0001902d"
    ;;
  *)
    echo "unsupported ROS distribution: ${ros_distro}" >&2
    exit 2
    ;;
esac
base_image="${RMW_CYCLONEDDS_C_BASE_IMAGE:-${default_base_image}}"

docker build --provenance=false \
  --build-arg "ROS_BASE_IMAGE=${base_image}" \
  --build-arg "ROS_DISTRO=${ros_distro}" \
  --tag "${image}" \
  "${repository_root}"
