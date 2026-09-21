# Inbound ROS graph profile

`rmw_cyclonedds_c` receives stock `rmw_cyclonedds_cpp` graph announcements and
joins them with Cyclone DDS built-in discovery. The graph message supplies the
participant-to-node and node-to-endpoint associations. Built-in participant,
publication, and subscription samples supply endpoint liveness, topic names,
and type names.

This is a bounded cache, not a C port of `rmw_dds_common::GraphCache`.

## Limits

| Resource | Default | CMake setting |
| --- | ---: | --- |
| Cached participants, total | 8 | `RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_PARTICIPANTS` |
| Cached nodes, total | 16 | `RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_NODES` |
| Cached DDS endpoints, total | 32 | `RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_ENDPOINTS` |
| Readers or writers associated with one node | 16 | `RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE` |
| Node, namespace, DDS topic, and DDS type names | 256 bytes | Graph protocol bound |

The totals include local state. All limits must be positive. An over-limit
graph snapshot or endpoint is ignored and emits
`RMW_CYCLONEDDS_C_GRAPH_LIMIT`; data is never truncated. The previously
accepted cache remains queryable.

Cyclone deserializes an incoming standard graph sample as a loan before the
implementation validates and copies it into the bounded cache. The retained
RMW graph state is fixed-capacity, but the transient receive allocation is
still owned by Cyclone DDS. Its runtime cost must be included in the ESP32
resource measurements.

## Supported APIs

The current API subset is:

```text
rmw_get_node_names()
rmw_get_topic_names_and_types()
rmw_count_publishers()
rmw_count_subscribers()
```

Normal topic queries report only `rt/` ROS topics and convert DDS names such
as `std_msgs::msg::dds_::UInt32_` to `std_msgs/msg/UInt32`. The
`no_demangle` form returns DDS topic and type names. Internal
`ros_discovery_info` endpoints are excluded.

Enclaves, per-node endpoint queries, endpoint-info arrays, services, clients,
and actions remain unsupported. In particular,
`rmw_get_node_names_with_enclaves()` is not implemented because the graph
sample does not carry enclave data and this profile does not parse security
metadata from participant user data.

Every accepted graph, participant, publication, or subscription change
signals the context graph guard condition. Participant disappearance removes
all nodes and endpoints owned by that participant.

## Validation

Run the stock-to-C lifecycle matrix with:

```sh
scripts/run_graph_inbound.sh
```

The Kilted matrix covers two remote participants, multiple remote nodes, the
same node name in different namespaces, multiple publishers and subscribers,
topic/type queries, rejection of unassociated plain-DDS endpoints, endpoint
removal, node shutdown, participant loss, and a restart with the same node
name. Four applicable bad-argument tests from upstream
`test_rmw_implementation/test_graph_api` also run in the conformance lane.

Lyrical builds and passes the package tests. Its stock graph publisher also
passes initial discovery and participant-loss cleanup. A Lyrical participant
containing several nodes can retain a destroyed node in its next graph
snapshot even after the node's DDS endpoints disappear. The C cache preserves
that received snapshot rather than guessing that an endpoint-free node is
dead; Kilted does not exhibit this behavior in the lifecycle matrix.

ESP32-S3 hardware acceptance completed against stock ROS 2 Lyrical on Zephyr
4.4.0. The inbound run passed participant loss and restart across three
participants, three nodes, and five remote endpoints; the complete graph and
QoS matrix also passed in both directions. The accepted resource measurements
and reproduction commands live in `ros2_zephyr/docs/graph-acceptance.md`.
