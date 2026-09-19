#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "${script_dir}/.." && pwd)"
output_dir="${1:-${repository_root}/results/graph-outbound}"
ros_distro="${RMW_CYCLONEDDS_C_ROS_DISTRO:-lyrical}"
image="${RMW_CYCLONEDDS_C_IMAGE:-servoagents/rmw-cyclonedds-c:${ros_distro}}"
domain_id="${ROS_DOMAIN_ID:-118}"
cyclonedds_uri="${RMW_CYCLONEDDS_C_URI:-<CycloneDDS><Domain><General><AllowMulticast>true</AllowMulticast></General></Domain></CycloneDDS>}"
fixture=/workspace/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/graph_fixture
working_dir="$(mktemp -d /tmp/rmw-cyclonedds-c-graph.XXXXXX)"
active_container=""

mkdir -p "${output_dir}"
: >"${output_dir}/summary.log"

cleanup() {
  if [[ -n "${active_container}" ]]; then
    docker rm -f "${active_container}" >/dev/null 2>&1 || true
  fi
  rm -rf "${working_dir}"
}
trap cleanup EXIT

run_cli() {
  docker run --rm --network bridge \
    -e ROS_DOMAIN_ID="${domain_id}" \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
    -e CYCLONEDDS_URI="${cyclonedds_uri}" \
    "${image}" \
    bash -lc ". /opt/ros/${ros_distro}/setup.sh && . /workspace/install/setup.sh && $*" 2>&1
}

wait_for_log() {
  local pattern="$1"
  for _ in $(seq 1 100); do
    if docker logs "${active_container}" 2>&1 | grep -q "${pattern}"; then
      return 0
    fi
    sleep 0.1
  done
  docker logs "${active_container}" >&2
  return 1
}

wait_for_node() {
  local expected="$1"
  local destination="$2"
  for _ in $(seq 1 20); do
    run_cli ros2 node list --no-daemon >"${destination}" || true
    if grep -qx "${expected}" "${destination}"; then
      return 0
    fi
    sleep 0.2
  done
  cat "${destination}" >&2
  return 1
}

wait_for_node_absence() {
  local unexpected="$1"
  local destination="$2"
  for _ in $(seq 1 20); do
    run_cli ros2 node list --no-daemon >"${destination}" || true
    if ! grep -qx "${unexpected}" "${destination}"; then
      return 0
    fi
    sleep 0.2
  done
  cat "${destination}" >&2
  return 1
}

start_fixture() {
  local topology="$1"
  local namespace_="$2"
  local case_dir="$3"
  active_container="rmw-c-graph-$$-${topology//_/-}"
  docker run --detach --name "${active_container}" --network bridge \
    -v "${case_dir}/control:/control" \
    -e ROS_DOMAIN_ID="${domain_id}" \
    -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
    -e CYCLONEDDS_URI="${cyclonedds_uri}" \
    "${image}" \
    bash -lc ". /opt/ros/${ros_distro}/setup.sh && . /workspace/install/setup.sh && exec ${fixture} ${topology} /control ${namespace_}" \
    >/dev/null
  wait_for_log "GRAPH_FIXTURE_READY topology=${topology}"
}

stop_fixture() {
  local case_dir="$1"
  touch "${case_dir}/control/stop"
  if ! docker wait "${active_container}" >"${case_dir}/status.log"; then
    docker logs "${active_container}" >"${case_dir}/fixture.log" 2>&1 || true
    return 1
  fi
  docker logs "${active_container}" >"${case_dir}/fixture.log" 2>&1 || true
  docker rm "${active_container}" >/dev/null
  active_container=""
  grep -q '^0$' "${case_dir}/status.log"
  grep -q '^GRAPH_FIXTURE_DONE$' "${case_dir}/fixture.log"
}

assert_topic_counts() {
  local topic="$1"
  local publishers="$2"
  local subscriptions="$3"
  local destination="$4"
  run_cli ros2 topic info "${topic}" --verbose >"${destination}"
  grep -q "^Publisher count: ${publishers}$" "${destination}"
  grep -q "^Subscription count: ${subscriptions}$" "${destination}"
}

run_static_case() {
  local topology="$1"
  local namespace_="${2:-/}"
  local expected_node=/graph_fixture
  local alpha_topic=/graph_alpha
  local beta_topic=/graph_beta
  if [[ "${namespace_}" != / ]]; then
    expected_node="${namespace_}/graph_fixture"
    alpha_topic="${namespace_}/graph_alpha"
    beta_topic="${namespace_}/graph_beta"
  fi
  local case_dir="${working_dir}/${topology}-${namespace_//\//_}"
  mkdir -p "${case_dir}/control"

  start_fixture "${topology}" "${namespace_}" "${case_dir}"
  wait_for_node "${expected_node}" "${case_dir}/node-list.log"
  run_cli ros2 node info "${expected_node}" --no-daemon >"${case_dir}/node-info.log"
  run_cli ros2 topic list >"${case_dir}/topic-list.log"

  case "${topology}" in
    node)
      ! grep -q '^/graph_' "${case_dir}/topic-list.log"
      ;;
    publisher)
      assert_topic_counts "${alpha_topic}" 1 0 "${case_dir}/graph-alpha.log"
      ;;
    subscriber)
      assert_topic_counts "${alpha_topic}" 0 1 "${case_dir}/graph-alpha.log"
      ;;
    both)
      assert_topic_counts "${alpha_topic}" 1 1 "${case_dir}/graph-alpha.log"
      ;;
    two_publishers)
      assert_topic_counts "${alpha_topic}" 2 0 "${case_dir}/graph-alpha.log"
      ;;
    two_subscriptions)
      assert_topic_counts "${alpha_topic}" 0 2 "${case_dir}/graph-alpha.log"
      ;;
    multiple_topics)
      assert_topic_counts "${alpha_topic}" 1 0 "${case_dir}/graph-alpha.log"
      assert_topic_counts "${beta_topic}" 0 1 "${case_dir}/graph-beta.log"
      ;;
  esac
  if grep -R -q 'Failed to parse type hash' "${case_dir}"; then
    echo "graph type hash warning in ${topology}" >&2
    return 1
  fi

  stop_fixture "${case_dir}"
  wait_for_node_absence "${expected_node}" "${case_dir}/node-list-after.log"
  mkdir -p "${output_dir}/${topology}-${namespace_//\//_}"
  cp -a "${case_dir}/." "${output_dir}/${topology}-${namespace_//\//_}/"
  echo "PASS topology=${topology} namespace=${namespace_}" | tee -a "${output_dir}/summary.log"
}

run_lifecycle_case() {
  local case_dir="${working_dir}/lifecycle"
  mkdir -p "${case_dir}/control"
  start_fixture lifecycle / "${case_dir}"
  wait_for_node /graph_fixture "${case_dir}/node-list.log"

  touch "${case_dir}/control/create_publisher"
  wait_for_log '^GRAPH_FIXTURE_STATE publisher$'
  assert_topic_counts /graph_alpha 1 0 "${case_dir}/publisher.log"

  touch "${case_dir}/control/create_subscription"
  wait_for_log '^GRAPH_FIXTURE_STATE publisher_subscription$'
  assert_topic_counts /graph_alpha 1 1 "${case_dir}/publisher-subscription.log"

  touch "${case_dir}/control/destroy_publisher"
  wait_for_log '^GRAPH_FIXTURE_STATE subscription$'
  assert_topic_counts /graph_alpha 0 1 "${case_dir}/subscription.log"

  touch "${case_dir}/control/destroy_subscription"
  wait_for_log '^GRAPH_FIXTURE_STATE node$'
  for _ in $(seq 1 20); do
    run_cli ros2 topic list >"${case_dir}/topic-list-after.log" || true
    if ! grep -q '^/graph_alpha$' "${case_dir}/topic-list-after.log"; then
      break
    fi
    sleep 0.2
  done
  if grep -q '^/graph_alpha$' "${case_dir}/topic-list-after.log"; then
    echo 'stale /graph_alpha topic after endpoint destruction' >&2
    return 1
  fi
  if grep -R -q 'Failed to parse type hash' "${case_dir}"; then
    echo 'graph type hash warning during lifecycle test' >&2
    return 1
  fi

  stop_fixture "${case_dir}"
  wait_for_node_absence /graph_fixture "${case_dir}/node-list-after.log"
  mkdir -p "${output_dir}/lifecycle"
  cp -a "${case_dir}/." "${output_dir}/lifecycle/"
  echo 'PASS endpoint lifecycle and stale removal' | tee -a "${output_dir}/summary.log"
}

run_static_case node
run_static_case publisher
run_static_case subscriber
run_static_case both
run_static_case two_publishers
run_static_case two_subscriptions
run_static_case multiple_topics
run_static_case publisher /graph_ns
run_lifecycle_case

echo "domain=${domain_id}" | tee -a "${output_dir}/summary.log"
