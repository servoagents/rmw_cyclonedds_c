# Stock Cyclone DDS graph reference

This note records the graph protocol emitted by stock `rmw_cyclonedds_cpp`
before graph support is added to `rmw_cyclonedds_c`. The primary reference is
ROS 2 Lyrical with `rmw_cyclonedds_cpp` 4.1.4 and `rmw_dds_common` 6.0.0,
because that is the desktop peer used by `ros2_zephyr`. The installed Kilted
packages use the same 16-byte `Gid` wire field.

The result comes from the tagged upstream sources, Cyclone's `finest` trace,
and simultaneous `ros2 node` and `ros2 topic` queries. It is a design input,
not an implementation in this repository.

## DDS contract

| Property | Stock value |
| --- | --- |
| DDS topic | `ros_discovery_info` |
| DDS type | `rmw_dds_common::msg::dds_::ParticipantEntitiesInfo_` |
| ROS namespace mangling | Disabled; the DDS topic has no `rt/` prefix |
| Type key | None |
| Writer reliability | Reliable, infinite max blocking time |
| Writer durability | Transient Local |
| Writer history | KEEP_LAST, depth 1 |
| Durability-service history | KEEP_LAST, depth 1; unlimited sample limits |
| Writer data lifecycle | Autodispose disabled |
| Internal reader reliability | Reliable |
| Internal reader durability | Transient Local |
| Internal reader history | KEEP_ALL |
| Local samples | Ignored at participant scope |
| Data representation | XCDR1 plain CDR |

The writer and reader history settings are intentionally asymmetric. The
source contains a FIXME to use a keyed graph type and KEEP_LAST(1) on the
reader. The current type is unkeyed, so the reader remains KEEP_ALL while each
participant's writer retains its latest full snapshot.

Lyrical advertises ROS type hash
`RIHS01_91a0593bacdcc50ea9bdcf849a938b128412cc1ea821245c663bcd26f83c295e`
for this endpoint. Its Cyclone trace reports minimal XTypes identifier
`d5a2a4d65f7a49b37ab3e7628202` and complete identifier
`2e82f50c0ec361f7a2abe38ff5cf`. These identifiers are reference-version
details; the IDL and serialized value are the compatibility contract.

## Message and serialization

The standard IDL is equivalent to:

```idl
struct Gid {
  uint8 data[16];
};

struct NodeEntitiesInfo {
  string<256> node_namespace;
  string<256> node_name;
  sequence<Gid> reader_gid_seq;
  sequence<Gid> writer_gid_seq;
};

struct ParticipantEntitiesInfo {
  Gid gid;
  sequence<NodeEntitiesInfo> node_entities_info_seq;
};
```

`rmw_cyclonedds_cpp` 4.1.4 permits XCDR1 only. On the observed little-endian
x86 host the four-byte encapsulation header is `00 01 00 00` (`CDR_LE`). The
payload then contains, in IDL order:

1. the 16 participant-GID bytes;
2. a four-byte node-sequence length;
3. for each node, namespace and name as CDR strings, each with a four-byte
   length that includes the terminating NUL and the required alignment;
4. a four-byte reader count followed by 16 bytes per reader GID;
5. a four-byte writer count followed by 16 bytes per writer GID.

There is no XCDR2 delimiter header or parameter list. The standard graph type
has bounded node strings but unbounded node and GID sequences. An embedded
implementation may impose explicit internal limits, but it must serialize the
same standard type and reject overflow rather than alter the wire type.

The graph sample does not carry topic names, type names, or endpoint QoS.
Those arrive through DDS built-in participant/publication/subscription
discovery. The graph sample associates those discovered endpoints with a ROS
node by GID.

## Identity representation

Cyclone copies each 16-byte DDS GUID directly into the message. No
`rmw_gid_t` implementation identifier is serialized.

- `ParticipantEntitiesInfo.gid` is the DDS participant GUID.
- Reader and writer entries are the DDS endpoint GUIDs.
- A ROS node has no independent GID. Its identity in this protocol is its
  participant plus the `node_namespace` and `node_name` entry.

The observed bytes printed by `ros2 topic info --verbose` exactly matched the
corresponding byte arrays in the graph trace. For example, the test publisher
was shown by the CLI as
`01.10.90.2c.69.8f.fa.f6.93.24.61.32.00.00.0e.03`, and the graph sample
contained the same 16 values. The final entity-kind byte was `03` for the
writer and `04` for the reader in this run.

## Announcement lifecycle

Every update is a full snapshot for the local DDS participant, not a delta.
The context mutex makes the local cache update and publish operation atomic.

| Operation | Published state |
| --- | --- |
| Context initialization | Graph writer/reader and built-in discovery readers are created; no ROS node entry is published yet. |
| Node creation | Adds one `NodeEntitiesInfo` and publishes the participant snapshot. A node with no endpoints has empty reader/writer sequences. |
| Publisher creation | Creates the DDS writer, appends its GUID to `writer_gid_seq`, then publishes the full snapshot. |
| Subscription creation | Creates the DDS reader, appends its GUID to `reader_gid_seq`, then publishes the full snapshot. |
| Publisher destruction | Publishes the snapshot without that writer GUID, then deletes the DDS writer. |
| Subscription destruction | Intended behavior is the equivalent reader update, with the Lyrical exception below. |
| Node destruction | Publishes the snapshot without that node. If it was the last node in the context, the graph endpoints and participant are then deleted. |
| Process loss | No final sample is possible; DDS participant disappearance removes the participant and its nodes from remote graph caches. |

The Transient Local writer makes the latest snapshot available to late
joiners while the participant remains alive. When several ROS nodes share an
RMW context, the sample contains one `NodeEntitiesInfo` per node and each
update republishes all of them.

### Lyrical 4.1.4 subscription-destruction quirk

The 4.1.4 source calls `remove_publisher_graph()` from
`rmw_destroy_subscription()` instead of `remove_subscriber_graph()`. The
runtime trace reproduced this: creating the test reader added its GID, but
destroying it emitted an unchanged graph snapshot before deleting the DDS
reader. This is reference behavior, not behavior to copy.

The ROS CLI still removed the subscription immediately because the stock
cache also consumes DDS built-in endpoint disposal and only reports live
endpoint records. A correct outbound implementation should remove the reader
GID in its announcement. Depending on built-in disposal to mask a stale
association would leave a bad retained snapshot for late graph readers.

## Runtime observation

The controlled node was `/graph_ns/graph_probe`; ROS-out and parameter
services were disabled where the `rclpy` API permits. Lyrical still created a
parameter-event publisher and a type-description service, so the trace was
used to separate those implicit endpoints from the controlled endpoints.

The trace sequence for the controlled changes was:

```text
node create       readers=[]                    writers=[]
publisher create  readers=[implicit service]    writers=[implicit..., test writer]
subscription add  readers=[implicit service,
                           test reader]          writers=[implicit..., test writer]
subscription del  unchanged snapshot            (4.1.4 quirk)
publisher del     test writer removed
node destroy      node_entities_info_seq=[]
```

At the same points:

- `ros2 node list --no-daemon` showed `/graph_ns/graph_probe` after node
  creation and no node after shutdown;
- `ros2 node info` added and removed `/graph_topic` in the appropriate
  publisher/subscriber section;
- `ros2 topic list` added `/graph_topic` when the first controlled endpoint
  appeared and removed it after the final one disappeared;
- `ros2 topic info /graph_topic --verbose` reported one endpoint of each kind,
  the same GIDs as the announcement, and the requested test depths 7 and 9.

The graph endpoint itself is not reported as a normal ROS topic because it
uses `avoid_ros_namespace_conventions` and serves the RMW implementation
internally.

## Source anchors

The audited source revisions are:

- `ros2/rmw_dds_common` tag `6.0.0`, commit
  `2f00e6e9208e925d61330ea92f857b2e67d595ab`:
  `msg/*.msg`, `src/context.cpp`, `src/graph_cache.cpp`, and
  `src/gid_utils.cpp`;
- `ros2/rmw_cyclonedds` tag `4.1.4`, commit
  `ace0c94fa6b856f5b9292dd037aca6fcca743ac8`:
  `rmw_cyclonedds_cpp/src/rmw_node.cpp`, `Serialization.cpp`, `serdata.cpp`,
  and `type_name.cpp`.

These are also the package versions used for the Lyrical runtime trace. Graph
implementation work should cross-check the active desktop distribution again
if either package version changes.
