#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

"${script_dir}/check.sh"
"${script_dir}/build_image.sh"
"${script_dir}/run_smoke.sh"
"${script_dir}/run_contract_tests.sh"
"${script_dir}/check_abi.sh"
"${script_dir}/run_uint32_interop.sh"
"${script_dir}/run_generated_interop.sh"
