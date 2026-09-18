# Transient Local profile

The embedded profile maps both supported RMW durability policies directly to
Cyclone DDS:

| RMW policy | Cyclone DDS policy |
| --- | --- |
| `RMW_QOS_POLICY_DURABILITY_VOLATILE` | `DDS_DURABILITY_VOLATILE` |
| `RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL` | `DDS_DURABILITY_TRANSIENT_LOCAL` |

Transient-local writers also set Cyclone's durability-service history to the
requested keep-last depth. Cyclone otherwise defaults that writer-cache depth
to one, independently of the endpoint history policy.

Transient Local does not widen the message or history profile. Endpoints still
require fixed-size generated types, finite nonzero depth, and `KEEP_LAST`.
`KEEP_ALL`, zero depth, default durability, variable-size messages, and silent
policy substitution remain unsupported.

## Contract

The package test creates the writer before the reader, publishes values 1
through 5, then creates the reader. At depth 1 the reader must receive retained
value 5; at depth 3 it must receive retained values 3, 4, and 5 in order. In
both cases value 6 is then published and must arrive as a live sample. Publisher
and subscription actual-QoS reports must contain reliable, transient-local,
keep-last and the requested depth.

The same cases are orchestrated against stock `rmw_cyclonedds_cpp` in both
directions:

```sh
scripts/run_transient_local.sh
```

The stock implementation is a wire-interoperability peer. RMW and DDS
semantics remain the specification.

## Acceptance state

The implementation and all Linux and Zephyr targets compile. The late-joiner,
stock-peer, memory-growth, and physical ESP32-S3 runs remain pending until the
test phase. Do not infer runtime behavior or memory cost from successful
compilation.
