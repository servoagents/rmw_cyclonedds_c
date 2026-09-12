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
| Best-effort, volatile, keep-last QoS | Supported |
| Scalar and fixed-array messages | Supported |
| Nested fixed-size messages | Supported |
| DDS-backed waits and guard conditions | Supported |
| Interoperability with `rmw_cyclonedds_cpp` | Tested |
| Reliable or transient-local QoS | Not supported |
| Strings and variable-size sequences | Not supported |
| Services, clients, and actions | Not supported |
| Remote graph queries and DDS Security | Not supported |

Unsupported policies and message shapes are rejected explicitly. The library
exports the complete RMW loader ABI required by ROS 2 Kilted, but functions
outside the supported profile return `RMW_RET_UNSUPPORTED` or the equivalent
failure value for their signature.

## Repository layout

```text
rmw_cyclonedds_c/                  RMW implementation
rosidl_typesupport_cyclonedds_c/   ROS C to Cyclone DDS C generator
cyclonedds_c_test_msgs/            test-only fixed-size interfaces
scripts/                           containerized test and interop lanes
```

## Build and test

The reproducible test lane uses Docker and ROS 2 Kilted:

```sh
scripts/test.sh
```

It builds all three packages, checks generator and RMW contracts, verifies the
loader ABI, and tests both wire directions against unmodified
`rmw_cyclonedds_cpp`. Results are written below `results/`.

For a local ROS workspace:

```sh
source /opt/ros/kilted/setup.sh
colcon build --packages-up-to rmw_cyclonedds_c
```

Set `RMW_IMPLEMENTATION=rmw_cyclonedds_c` before running an application.

## Relationship to ros2_zephyr

[`ros2_zephyr`](https://github.com/servoagents/ros2_zephyr) cross-builds these
packages as part of a ROS 2 C runtime for Zephyr. Neither the RMW nor its type
support is Zephyr-specific; Linux tests remain part of their public contract.

## License

Apache License 2.0. See [LICENSE](LICENSE).

ROS 2 is a trademark of Open Robotics. Eclipse Cyclone DDS is an Eclipse
Foundation project. This repository is not endorsed by either organization.
