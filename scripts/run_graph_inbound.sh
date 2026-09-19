#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "${script_dir}/.." && pwd)"
output_dir="${1:-${repository_root}/results/graph-inbound}"
ros_distro="${RMW_CYCLONEDDS_C_ROS_DISTRO:-kilted}"
image="${RMW_CYCLONEDDS_C_IMAGE:-servoagents/rmw-cyclonedds-c:${ros_distro}}"
domain_id="${ROS_DOMAIN_ID:-130}"
cyclonedds_uri="${RMW_CYCLONEDDS_C_URI:-<CycloneDDS><Domain><General><AllowMulticast>true</AllowMulticast></General></Domain></CycloneDDS>}"
fixture=/workspace/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/graph_query_fixture
peer=/workspace/src/scripts/graph_peer.py
working_dir="$(mktemp -d /tmp/rmw-cyclonedds-c-graph-inbound.XXXXXX)"
query_container="rmw-c-graph-query-${BASHPID}"
primary_container="rmw-c-graph-primary-${BASHPID}"
secondary_container="rmw-c-graph-secondary-${BASHPID}"

mkdir -p "${output_dir}" "${working_dir}/primary" "${working_dir}/secondary"

cleanup() {
  docker logs "${primary_container}" >"${output_dir}/primary.log" 2>&1 || true
  docker logs "${secondary_container}" >"${output_dir}/secondary.log" 2>&1 || true
  docker logs "${query_container}" >"${output_dir}/query.log" 2>&1 || true
  docker rm -f "${query_container}" "${primary_container}" "${secondary_container}" \
    >/dev/null 2>&1 || true
  rm -rf "${working_dir}"
}
trap cleanup EXIT

wait_for_log() {
  local container="$1"
  local pattern="$2"
  for _ in $(seq 1 400); do
    if docker logs "${container}" 2>&1 | grep -q "${pattern}"; then
      return 0
    fi
    sleep 0.1
  done
  docker logs "${container}" >&2
  return 1
}

start_peer() {
  local role="$1"
  local container="$2"
  local control="$3"
  docker run --detach --name "${container}" --network bridge \
    -v "${control}:/control" \
    -e ROS_DOMAIN_ID="${domain_id}" \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
    -e CYCLONEDDS_URI="${cyclonedds_uri}" \
    "${image}" \
    bash -lc ". /opt/ros/${ros_distro}/setup.sh && exec python3 ${peer} ${role} /control" \
    >/dev/null
  wait_for_log "${container}" "GRAPH_PEER_READY role=${role}"
}

docker run --detach --name "${query_container}" --network bridge \
  -e ROS_DOMAIN_ID="${domain_id}" \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
  -e CYCLONEDDS_URI="${cyclonedds_uri}" \
  "${image}" \
  bash -lc ". /opt/ros/${ros_distro}/setup.sh && . /workspace/install/setup.sh && exec ${fixture}" \
  >/dev/null

start_peer primary "${primary_container}" "${working_dir}/primary"
start_peer secondary "${secondary_container}" "${working_dir}/secondary"
wait_for_log "${query_container}" '^GRAPH_QUERY_PASS state=initial$'

touch "${working_dir}/secondary/stop"
docker wait "${secondary_container}" >"${output_dir}/secondary.status"
wait_for_log "${query_container}" '^GRAPH_QUERY_PASS state=secondary_gone$'

touch "${working_dir}/primary/drop"
wait_for_log "${primary_container}" '^GRAPH_PEER_STATE role=primary state=drop$'
wait_for_log "${query_container}" '^GRAPH_QUERY_PASS state=primary_drop$'

touch "${working_dir}/primary/restart"
wait_for_log "${primary_container}" '^GRAPH_PEER_STATE role=primary state=restart$'
wait_for_log "${query_container}" '^GRAPH_QUERY_PASS state=primary_restart$'

touch "${working_dir}/primary/stop"
docker wait "${primary_container}" >"${output_dir}/primary.status"
wait_for_log "${query_container}" '^GRAPH_QUERY_PASS state=all_remote_gone$'
docker wait "${query_container}" >"${output_dir}/query.status"

docker logs "${primary_container}" >"${output_dir}/primary.log" 2>&1
docker logs "${secondary_container}" >"${output_dir}/secondary.log" 2>&1
docker logs "${query_container}" >"${output_dir}/query.log" 2>&1

grep -q '^0$' "${output_dir}/primary.status"
grep -q '^0$' "${output_dir}/secondary.status"
grep -q '^0$' "${output_dir}/query.status"
if grep -Eq 'RMW_CYCLONEDDS_C_GRAPH_(ERROR|LIMIT)' "${output_dir}/query.log"; then
  cat "${output_dir}/query.log" >&2
  exit 1
fi

cat >"${output_dir}/summary.log" <<EOF
PASS remote nodes across two participants
PASS duplicate node name in distinct namespaces
PASS publisher/subscriber counts and topic types
PASS DDS endpoints without a ROS graph association remain hidden
PASS endpoint deletion and node shutdown
PASS participant disappearance
PASS restart with the same node name
domain=${domain_id}
EOF
cat "${output_dir}/summary.log"
