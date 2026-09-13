#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export RMW_CYCLONEDDS_C_ROS_DISTRO="${RMW_CYCLONEDDS_C_ROS_DISTRO:-lyrical}"

"${script_dir}/check.sh"
"${script_dir}/build_image.sh"
"${script_dir}/run_smoke.sh"
"${script_dir}/run_contract_tests.sh"
"${script_dir}/check_abi.sh"
