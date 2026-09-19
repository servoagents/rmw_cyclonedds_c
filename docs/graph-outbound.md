# Outbound ROS graph profile

`rmw_cyclonedds_c` publishes the local participant snapshot expected by
`rmw_dds_common`. This milestone is intentionally one-way: stock ROS nodes can
see C RMW nodes and endpoints, but the C RMW does not yet subscribe to graph
announcements or answer remote graph queries.

## Representation

Graph traffic uses a private Cyclone IDLC descriptor for
`rmw_dds_common::msg::dds_::ParticipantEntitiesInfo_`. The descriptor retains
the standard unbounded DDS sequences and 256-byte bounded strings on the wire.
Its member names match the ROS introspection type used by
`rmw_cyclonedds_cpp`, rather than the escaped C identifiers emitted by the ROS
DDS IDL generator.

The runtime state itself is bounded and allocated with the RMW context. It
does not use the application message conversion path and does not add string
or variable-sequence support to `rosidl_typesupport_cyclonedds_c`.

The default limits are:

| Resource | Default | CMake setting |
| --- | ---: | --- |
| Local nodes per context | 8 | `RMW_CYCLONEDDS_C_GRAPH_MAX_NODES` |
| Publishers per node | 16 | `RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE` |
| Subscriptions per node | 16 | `RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE` |
| Node name and namespace | 256 bytes each | Standard graph type bound |

Creation fails explicitly when a limit is reached. Names are never truncated.

## DDS behavior

The writer uses the stock graph topic, type hash, and QoS:

```text
topic:              ros_discovery_info
type:               rmw_dds_common::msg::dds_::ParticipantEntitiesInfo_
reliability:        reliable
durability:         transient local
history:            keep last, depth 1
durability service: keep last, depth 1
autodispose:         disabled
representation:     XCDR1
```

Every change publishes a complete participant snapshot. Node creation adds an
empty node entry. Publisher and subscription creation append the actual
16-byte Cyclone endpoint GUID to the corresponding sequence. Endpoint removal
is announced before the DDS entity is deleted. Node destruction publishes the
snapshot without that node. Participant loss remains the fallback cleanup for
an ungraceful process exit.

Application publisher GIDs now use the same 16-byte DDS GUID representation,
instead of a local Cyclone instance handle.

## Validation

The stock CLI matrix is run with:

```sh
scripts/run_graph_outbound.sh
```

It checks node-only, publisher-only, subscriber-only, combined, duplicate
publisher/subscriber, multiple-topic, and non-root namespace topologies. A
controlled lifecycle case adds and removes endpoints while stock
`rmw_cyclonedds_cpp` queries the graph, then verifies that endpoints and the
node leave no stale entries.

The matrix passes against ROS 2 Lyrical and Kilted. Existing Kilted data-path
tests also remain green for Best Effort, Reliable, and Transient Local depths
1 and 3. Lyrical graph exchange works because the private graph descriptor
matches its dynamic graph type; the separate application-type XTypes mismatch
described in [the Lyrical note](lyrical.md) is unchanged.

Inbound graph state, graph query APIs, services, and actions are outside this
milestone.
