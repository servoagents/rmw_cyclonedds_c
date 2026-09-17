#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "${script_dir}/.." && pwd)"
ros_distro="${RMW_CYCLONEDDS_C_ROS_DISTRO:-kilted}"
image="${RMW_CYCLONEDDS_C_IMAGE:-servoagents/rmw-cyclonedds-c:${ros_distro}}"
output_dir="${1:-${repository_root}/results/conformance-${ros_distro}}"
test_pattern='^(test_init_shutdown|test_init_options|test_create_destroy_node|test_publisher_allocator|test_subscription_allocator)__rmw_cyclonedds_c$'

mkdir -p "${output_dir}"

docker run --rm \
  --memory="${RMW_CYCLONEDDS_C_CONFORMANCE_MEMORY:-3g}" \
  --user "$(id -u):$(id -g)" \
  -e CMAKE_BUILD_PARALLEL_LEVEL=1 \
  -e HOME=/tmp \
  -v "${output_dir}:/results" \
  "${image}" \
  bash -lc "source /opt/ros/${ros_distro}/setup.sh
source /workspace/install/setup.sh
export RMW_IMPLEMENTATIONS=rmw_cyclonedds_c
colcon --log-base /results/log build \
  --executor sequential \
  --base-paths /opt/test_rmw_implementation \
  --build-base /results/build \
  --install-base /results/install \
  --packages-select test_rmw_implementation \
  --event-handlers console_cohesion+
ctest --test-dir /results/build/test_rmw_implementation \
  --output-on-failure \
  -R '${test_pattern}'
colcon --log-base /results/log-result test-result \
  --test-result-base /results/build/test_rmw_implementation \
  --verbose" |& tee "${output_dir}/summary.log"
