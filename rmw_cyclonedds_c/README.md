# rmw_cyclonedds_c

This package implements a fixed ROS 2 RMW profile directly on the Cyclone DDS
C API. It owns the Cyclone participant and endpoints; it does not wrap
`rmw_cyclonedds_cpp`.

The supported runtime surface includes initialization and cleanup, nodes,
publishers, subscriptions, publish/take, DDS-backed wait sets, guard
conditions, GIDs, endpoint match counts, and QoS validation. Generated scalar,
fixed-array, and nested fixed-size messages are supported through
`rosidl_typesupport_cyclonedds_c`. A built-in `std_msgs/msg/UInt32` adapter is
kept for compatibility with prebuilt interface packages.

The implementation accepts best-effort or reliable, volatile or
transient-local, finite keep-last QoS. It publishes bounded local node and
endpoint state on the standard ROS graph topic. It rejects strings and
variable-size application sequences, content filters, loaned and serialized
messages, services, clients, events, inbound graph queries, and DDS Security.

The dynamic ROS 2 loader resolves the complete RMW ABI. Entry points outside
the supported profile are present but return `RMW_RET_UNSUPPORTED`, `NULL`, or
`false`, as appropriate. They never return a placeholder success result.

See the [repository README](../README.md) for build and interoperability tests.
