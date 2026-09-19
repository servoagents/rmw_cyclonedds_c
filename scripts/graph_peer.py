#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0

import argparse
import pathlib
import time

import rclpy
from rclpy.executors import SingleThreadedExecutor
from rclpy.node import Node
from std_msgs.msg import UInt32


def command_exists(control: pathlib.Path, command: str) -> bool:
    return (control / command).exists()


def primary_peer(control: pathlib.Path) -> None:
    executor = SingleThreadedExecutor()
    alpha = Node(
        "same_name", namespace="/alpha", enable_rosout=False, start_parameter_services=False
    )
    beta = Node(
        "same_name", namespace="/beta", enable_rosout=False, start_parameter_services=False
    )
    gamma = Node(
        "third", namespace="/gamma", enable_rosout=False, start_parameter_services=False
    )
    for node in (alpha, beta, gamma):
        executor.add_node(node)

    alpha_publishers = [
        alpha.create_publisher(UInt32, "/graph_remote_a", 10),
        alpha.create_publisher(UInt32, "/graph_remote_a", 10),
    ]
    beta_subscriptions = [
        beta.create_subscription(UInt32, "/graph_remote_a", lambda _: None, 10),
        beta.create_subscription(UInt32, "/graph_remote_a", lambda _: None, 10),
    ]
    gamma.create_publisher(UInt32, "/graph_remote_b", 10)
    print("GRAPH_PEER_READY role=primary", flush=True)

    dropped = False
    restarted = False
    while not command_exists(control, "stop"):
        executor.spin_once(timeout_sec=0.05)
        if not dropped and command_exists(control, "drop"):
            alpha.destroy_publisher(alpha_publishers.pop())
            for subscription in beta_subscriptions:
                beta.destroy_subscription(subscription)
            beta_subscriptions.clear()
            executor.remove_node(beta)
            beta.destroy_node()
            dropped = True
            print("GRAPH_PEER_STATE role=primary state=drop", flush=True)
        if dropped and not restarted and command_exists(control, "restart"):
            beta = Node(
                "same_name",
                namespace="/beta",
                enable_rosout=False,
                start_parameter_services=False,
            )
            executor.add_node(beta)
            beta_subscriptions = [
                beta.create_subscription(UInt32, "/graph_remote_a", lambda _: None, 10),
                beta.create_subscription(UInt32, "/graph_remote_a", lambda _: None, 10),
            ]
            restarted = True
            print("GRAPH_PEER_STATE role=primary state=restart", flush=True)

    for node in list(executor.get_nodes()):
        executor.remove_node(node)
        node.destroy_node()


def secondary_peer(control: pathlib.Path) -> None:
    node = Node(
        "separate", namespace="/other", enable_rosout=False, start_parameter_services=False
    )
    node.create_publisher(UInt32, "/graph_remote_c", 10)
    print("GRAPH_PEER_READY role=secondary", flush=True)
    while not command_exists(control, "stop"):
        rclpy.spin_once(node, timeout_sec=0.05)
    node.destroy_node()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("role", choices=("primary", "secondary"))
    parser.add_argument("control", type=pathlib.Path)
    args = parser.parse_args()
    rclpy.init()
    try:
        if args.role == "primary":
            primary_peer(args.control)
        else:
            secondary_peer(args.control)
    finally:
        rclpy.shutdown()


if __name__ == "__main__":
    main()
