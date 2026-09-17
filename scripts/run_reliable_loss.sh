#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd "${script_dir}/.." && pwd)"
output_dir="${1:-${repository_root}/results/reliable-loss}"
ros_distro="${RMW_CYCLONEDDS_C_ROS_DISTRO:-kilted}"
image="${RMW_CYCLONEDDS_C_IMAGE:-servoagents/rmw-cyclonedds-c:${ros_distro}}"
domain_id="${ROS_DOMAIN_ID:-96}"
cyclonedds_uri="${RMW_CYCLONEDDS_C_URI:-<CycloneDDS><Domain><General><AllowMulticast>true</AllowMulticast></General></Domain></CycloneDDS>}"
binary=/workspace/install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/reliable_loss
publisher_name="rmw-reliable-loss-${BASHPID}"

mkdir -p "${output_dir}"

subscriber_pid=""
publisher_pid=""
cleanup() {
  [[ -z "${subscriber_pid}" ]] || kill "${subscriber_pid}" 2>/dev/null || true
  [[ -z "${publisher_pid}" ]] || kill "${publisher_pid}" 2>/dev/null || true
  docker rm --force "${publisher_name}" >/dev/null 2>&1 || true
}
trap cleanup EXIT

timeout --signal=KILL 45 docker run --rm --network bridge \
  -e ROS_DOMAIN_ID="${domain_id}" \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
  -e CYCLONEDDS_URI="${cyclonedds_uri}" \
  "${image}" \
  bash -lc ". /opt/ros/${ros_distro}/setup.sh && . /workspace/install/setup.sh && exec ${binary} sub" \
  >"${output_dir}/subscriber.log" 2>&1 &
subscriber_pid=$!

timeout --signal=KILL 45 docker run --rm --network bridge --cap-add NET_ADMIN \
  --name "${publisher_name}" \
  -e ROS_DOMAIN_ID="${domain_id}" \
  -e RMW_IMPLEMENTATION=rmw_cyclonedds_c \
  -e CYCLONEDDS_URI="${cyclonedds_uri}" \
  "${image}" \
  bash -lc ". /opt/ros/${ros_distro}/setup.sh && . /workspace/install/setup.sh && exec ${binary} pub" \
  >"${output_dir}/publisher.log" 2>&1 &
publisher_pid=$!

for _ in {1..300}; do
  grep -q '^RMW_RELIABLE_LOSS_MATCH role=pub$' "${output_dir}/publisher.log" && break
  sleep 0.1
done
grep -q '^RMW_RELIABLE_LOSS_MATCH role=pub$' "${output_dir}/publisher.log"

docker exec "${publisher_name}" tc qdisc add dev eth0 root netem loss 100%
for _ in {1..100}; do
  grep -q '^RMW_RELIABLE_LOSS_SENT sequence=1$' "${output_dir}/publisher.log" && break
  sleep 0.1
done
grep -q '^RMW_RELIABLE_LOSS_SENT sequence=1$' "${output_dir}/publisher.log"
sleep 0.5
if grep -q '^RMW_RELIABLE_LOSS_RECEIVED sequence=1$' "${output_dir}/subscriber.log"; then
  echo "first sample arrived while publisher egress was fully impaired" >&2
  exit 1
fi
docker exec "${publisher_name}" tc -s qdisc show dev eth0 >"${output_dir}/netem.log"
grep -Eq 'dropped [1-9][0-9]*' "${output_dir}/netem.log"
docker exec "${publisher_name}" tc qdisc del dev eth0 root

wait "${publisher_pid}"
publisher_pid=""
wait "${subscriber_pid}"
subscriber_pid=""

grep -q '^RMW_RELIABLE_LOSS_PASS role=pub samples=5$' "${output_dir}/publisher.log"
grep -q '^RMW_RELIABLE_LOSS_PASS role=sub samples=5 first_sample_recovered=1$' \
  "${output_dir}/subscriber.log"

echo "PASS reliable first sample recovered after deterministic publisher egress loss"
echo "domain=${domain_id}"
