#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export RMW_CYCLONEDDS_C_ROS_DISTRO="${RMW_CYCLONEDDS_C_ROS_DISTRO:-kilted}"

"${script_dir}/test_core.sh"
"${script_dir}/run_graph_outbound.sh"
"${script_dir}/run_graph_inbound.sh"
"${script_dir}/run_uint32_interop.sh"
"${script_dir}/run_generated_interop.sh"
"${script_dir}/run_reliable_loss.sh"
"${script_dir}/run_transient_local.sh"
