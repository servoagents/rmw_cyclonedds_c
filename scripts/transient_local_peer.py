#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0

"""Stock rmw_cyclonedds_cpp peer for transient-local late-joiner tests."""

import argparse
import sys
import time

import rclpy
from cyclonedds_c_test_msgs.msg import NestedFixed
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy

HISTORY_LAST_VALUE = 5
LIVE_VALUE = 6


def endpoint_qos(depth: int) -> QoSProfile:
    return QoSProfile(
        depth=depth,
        durability=DurabilityPolicy.TRANSIENT_LOCAL,
        history=HistoryPolicy.KEEP_LAST,
        reliability=ReliabilityPolicy.RELIABLE,
    )


def message_with_value(value: int) -> NestedFixed:
    message = NestedFixed()
    message.counter.data = value
    message.samples.values = [value * 10 + index for index in range(4)]
    return message


def wait_for_match(node, endpoint, timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        rclpy.spin_once(node, timeout_sec=0.1)
        if endpoint.get_subscription_count() > 0:
            return True
    return False


def wait_for_subscription_cleanup(node, publisher, timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        rclpy.spin_once(node, timeout_sec=0.1)
        if publisher.get_subscription_count() == 0:
            return True
    return False


def run_publisher(node, depth: int, timeout: float) -> int:
    topic = f"/transient_local_ros_to_rmw_depth_{depth}"
    publisher = node.create_publisher(NestedFixed, topic, endpoint_qos(depth))
    for value in range(1, HISTORY_LAST_VALUE + 1):
        publisher.publish(message_with_value(value))
        rclpy.spin_once(node, timeout_sec=0.02)
    print(
        f"STOCK_TRANSIENT_LOCAL_READY role=pub depth={depth} "
        f"history_last={HISTORY_LAST_VALUE}",
        flush=True,
    )
    if not wait_for_match(node, publisher, timeout):
        print(
            f"STOCK_TRANSIENT_LOCAL_ERROR role=pub depth={depth} reason=match_timeout",
            file=sys.stderr,
        )
        return 1
    history_deadline = time.monotonic() + 0.5
    while time.monotonic() < history_deadline:
        rclpy.spin_once(node, timeout_sec=0.05)
    publisher.publish(message_with_value(LIVE_VALUE))
    if not wait_for_subscription_cleanup(node, publisher, timeout):
        print(
            f"STOCK_TRANSIENT_LOCAL_ERROR role=pub depth={depth} "
            "reason=subscriber_cleanup_timeout",
            file=sys.stderr,
        )
        return 1
    print(
        f"STOCK_TRANSIENT_LOCAL_PASS direction=stock_to_c depth={depth} live={LIVE_VALUE}"
    )
    return 0


def run_subscriber(node, depth: int, timeout: float) -> int:
    topic = f"/transient_local_rmw_to_ros_depth_{depth}"
    received: list[int] = []

    def receive(message: NestedFixed) -> None:
        received.append(message.counter.data)

    subscription = node.create_subscription(
        NestedFixed, topic, receive, endpoint_qos(depth)
    )
    expected = list(range(HISTORY_LAST_VALUE + 1 - depth, LIVE_VALUE + 1))
    deadline = time.monotonic() + timeout
    while len(received) < len(expected) and time.monotonic() < deadline:
        rclpy.spin_once(node, timeout_sec=0.1)
    node.destroy_subscription(subscription)
    if received != expected:
        print(
            f"STOCK_TRANSIENT_LOCAL_ERROR role=sub depth={depth} "
            f"expected={expected} received={received}",
            file=sys.stderr,
        )
        return 1
    print(
        f"STOCK_TRANSIENT_LOCAL_PASS direction=c_to_stock depth={depth} "
        f"history_first={expected[0]} history_last={HISTORY_LAST_VALUE} live={LIVE_VALUE}"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("role", choices=("pub", "sub"))
    parser.add_argument("--depth", type=int, choices=(1, 3), required=True)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()

    rclpy.init()
    node = rclpy.create_node(f"stock_transient_local_{args.role}_{args.depth}")
    try:
        if args.role == "pub":
            return run_publisher(node, args.depth, args.timeout)
        return run_subscriber(node, args.depth, args.timeout)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    raise SystemExit(main())
