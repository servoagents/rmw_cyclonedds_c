#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

repository_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

git -C "${repository_root}" diff --check

while IFS= read -r -d '' script; do
  bash -n "${script}"
done < <(find "${repository_root}" -path "${repository_root}/.git" -prune \
  -o -type f -name '*.sh' -print0)

python3 -m compileall -q \
  "${repository_root}/rosidl_typesupport_cyclonedds_c" \
  "${repository_root}/scripts"

if command -v shellcheck >/dev/null; then
  find "${repository_root}" -path "${repository_root}/.git" -prune \
    -o -type f -name '*.sh' -print0 | xargs -0 shellcheck
else
  echo "warning: shellcheck is not installed; skipped shell lint" >&2
fi
