# rmw_cyclonedds_c

`rmw_cyclonedds_c` is a C implementation of the ROS 2 RMW interface backed by
Eclipse Cyclone DDS. It is designed for static linking and bounded embedded
deployments, while remaining usable and testable on Linux.

The repository also contains `rosidl_typesupport_cyclonedds_c`, the matching C
type-support generator. Keeping both packages together gives the supported
message profile one release and interoperability contract.

## Status

This is a profile-limited implementation, not a drop-in replacement for a
desktop RMW.

| Capability | Status |
| --- | --- |
| Publishers and subscriptions | Supported |
| Best-effort or reliable QoS | Supported |
| Volatile, finite keep-last QoS | Supported |
| Transient-local, finite keep-last QoS | Supported and tested at depths 1 and 3 |
| Scalar and fixed-array messages | Supported |
| Nested fixed-size messages | Supported |
| DDS-backed waits and guard conditions | Supported |
| Local graph announcements | Supported; visible to stock ROS graph tools |
| Interoperability with `rmw_cyclonedds_cpp` | Tested on Kilted and on the embedded Lyrical boundary |
| Strings and variable-size sequences | Not supported |
| Services, clients, and actions | Not supported |
| Remote graph reception, graph queries, and DDS Security | Not supported |

Unsupported policies and message shapes are rejected explicitly. The library
exports the complete RMW loader ABI required by ROS 2 Kilted and Lyrical, but
functions outside the supported profile return `RMW_RET_UNSUPPORTED` or the
equivalent failure value for their signature.

## Repository layout

```text
rmw_cyclonedds_c/                  RMW implementation
rosidl_typesupport_cyclonedds_c/   ROS C to Cyclone DDS C generator
cyclonedds_c_test_msgs/            test-only fixed-size interfaces
scripts/                           containerized test and interop lanes
```

## Build and test

The complete reproducible lane uses Docker and ROS 2 Kilted:

```sh
scripts/test.sh
```

It builds all three packages, checks generator and RMW contracts, verifies the
loader ABI, and tests best-effort and reliable communication in both wire
directions against stock `rmw_cyclonedds_cpp`. It also checks local graph
topologies and endpoint lifecycles through the stock ROS CLI. The stock
implementation is an interoperability reference, not the behavioral
specification: the RMW API, ROS 2 semantics, and DDS semantics define the
contract. The Reliable lane also drops the first sample on the publisher's
network interface and verifies that the same sample is delivered after
Cyclone retransmits it. The Transient Local lane is set up to start each
publisher first, then verify retained history and a live sample at depths 1
and 3 with both a C RMW peer and stock `rmw_cyclonedds_cpp`. Results are
written below `results/`.

The Lyrical migration currently covers compilation, the loader ABI, the RMW
smoke test, and negative/timeout/guard-condition contracts:

```sh
scripts/test_core.sh
```

Wire interoperability with a normal, TypeInformation-enabled stock Lyrical
`rmw_cyclonedds_cpp` is not claimed. Lyrical's installed DDS IDL and its
dynamic type construction use different member names, which produces different
XTypes identifiers for the same ROS message. The accepted ESP32-S3 Wi-Fi lane
uses the embedded Cyclone build without type discovery; that distinct boundary
is documented in [the Lyrical compatibility note](docs/lyrical.md).

For a local ROS workspace:

```sh
source /opt/ros/kilted/setup.sh
colcon build --packages-up-to rmw_cyclonedds_c
colcon test --packages-select rmw_cyclonedds_c
colcon test-result --verbose
```

Set `RMW_IMPLEMENTATION=rmw_cyclonedds_c` before running an application.
The package test runner covers the self-contained smoke, contract, and
best-effort/reliable C-to-C tests and the Transient Local late-joiner contract.
Stock-RMW interoperability, network fault injection, cross-distribution
checks, and hardware runs remain explicit orchestration scripts.

The focused Transient Local interoperability lane is:

```sh
scripts/run_transient_local.sh
```

See [the Transient Local profile](docs/transient-local.md) for the supported
boundary and acceptance cases.

The stock graph wire contract and the bounded outbound implementation are
documented in [the graph reference](docs/graph-reference.md) and
[the outbound graph profile](docs/graph-outbound.md). Run its stock CLI and
lifecycle matrix with:

```sh
scripts/run_graph_outbound.sh
```

The implementation-neutral ROS `test_rmw_implementation` suite is tracked as
a separate conformance lane. Tests for the supported profile must pass;
tests requiring deliberately unsupported APIs are recorded as such rather
than treated as successful coverage. That boundary should shrink as the RMW
API grows. See [the conformance boundary](docs/conformance.md) for the pinned
suite, command, and current classification.

## Relationship to ros2_zephyr

[`ros2_zephyr`](https://github.com/servoagents/ros2_zephyr) cross-builds these
packages as part of a ROS 2 C runtime for Zephyr. Neither the RMW nor its type
support is Zephyr-specific; Linux tests remain part of their public contract.

## License

Apache License 2.0. See [LICENSE](LICENSE).

ROS 2 is a trademark of Open Robotics. Eclipse Cyclone DDS is an Eclipse
Foundation project. This repository is not endorsed by either organization.
