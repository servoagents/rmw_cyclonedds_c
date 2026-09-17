# RMW conformance boundary

The implementation-neutral `test_rmw_implementation` suite is the normative
API check. Stock `rmw_cyclonedds_cpp` remains the wire-interoperability peer;
it is not a behavioral specification for this implementation.

The upstream test source is pinned per ROS distribution:

| ROS distribution | `rmw_implementation` revision |
| --- | --- |
| Kilted | `1a9b0e672a787af8c3a740d21dece9389d6b77ea` |
| Lyrical | `e241da2fbf75f9f8676751108d18d91115cdbe04` |

Run the current Kilted lane with:

```sh
RMW_CYCLONEDDS_C_ROS_DISTRO=kilted scripts/build_image.sh
scripts/run_conformance.sh
```

Set `RMW_CYCLONEDDS_C_ROS_DISTRO=lyrical` on both commands to check Lyrical.
The runner uses one build worker and a 3 GiB container limit. Its artifacts
and JUnit files are written below `results/conformance-<distro>/`.

## Current classification

| Upstream suite | Classification | Reason |
| --- | --- | --- |
| `test_init_shutdown` | Must pass | Initialization, shutdown, and context lifecycle are supported. |
| `test_init_options` | Must pass | Initialization options are supported. |
| `test_create_destroy_node` | Must pass | Local node lifecycle is supported. |
| `test_publisher_allocator` | Upstream skip | Publisher loan/allocation API is unsupported. |
| `test_subscription_allocator` | Upstream skip | Subscription loan/allocation API is unsupported. |
| `test_publisher`, `test_subscription` | Not selected | Upstream `test_msgs` lacks this profile's generated fixed-size adapter; local generated-message tests cover supported endpoints. |
| `test_serialize_deserialize` | Not selected | Serialized-message APIs are unsupported. |
| `test_wait_set` | Not selected | The suite fixture requires unsupported services and events; local tests cover subscriptions, guard conditions, and timeouts. |
| `test_graph_api` | Not selected | Remote graph queries are unsupported. |
| `test_unique_identifiers` | Not selected | The suite requires unsupported clients. |
| `test_service`, `test_client` | Not selected | Services and clients are unsupported. |
| `test_qos_profile_check_compatible` | Not selected | The compatibility-query API is unsupported. |
| `test_duration_infinite` | Not selected | The suite uses an upstream message without this profile's generated adapter. |
| `test_event` | Not selected | RMW events are unsupported. |

An unselected suite is not counted as a pass. When its prerequisite API enters
the supported profile, move it into the runner's must-pass set. Unexpected
failures in the selected set are bugs.
