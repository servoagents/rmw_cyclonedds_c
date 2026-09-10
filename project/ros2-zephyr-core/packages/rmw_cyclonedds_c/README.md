# Experimental Cyclone DDS C RMW

This package is the Phase 3 Linux-first vertical slice. It is intentionally not
a complete RMW implementation.

The initial supported profile is:

- ordinary `rclc`/`rcl` initialization and cleanup;
- one shared Cyclone DDS participant per RMW context;
- nodes and their required graph guard condition;
- `std_msgs/msg/UInt32` publishers and subscriptions;
- ROS topic naming (`rt/<absolute-topic>`);
- best-effort, volatile, keep-last QoS;
- publish, take-with-info, and DDS-backed waitsets;
- application guard-condition wakeup, GIDs, and endpoint match counts;
- no RMW-created callback thread and no extra message queue.

Other message types, reliable/transient-local QoS, content filters, loaning,
services, clients, events, and remote graph queries are outside this slice.
Creation calls reject unsupported types and policies. The package must not be
presented as a general-purpose RMW. Kilted resolves its complete dynamic RMW
dispatch table, so the library exports that ABI; operations outside this
profile return `RMW_RET_UNSUPPORTED` (or `NULL`/`false` for their API shape)
with no fake success.

## Reproducible Linux workflow

From the lab root:

```sh
scripts/phase3.sh all
```

This builds the pinned Kilted peer image, installs the pinned `rclc` and
`ltrace` packages into the Phase 3 image, compiles the package, traces the
minimal program against the reference C++ Cyclone RMW, runs the same ordinary
`rclc` program with `RMW_IMPLEMENTATION=rmw_cyclonedds_c`, exercises negative
contracts and guard-condition waits, and tests both directions against the
unmodified C++ Cyclone RMW.

Individual actions are `image`, `trace`, `smoke`, `contract`, `interop`, and
`metrics`. Runtime logs are ignored under `results/phase3-*`. The Linux metrics
record ELF section sizes and `/proc` process high-water/RSS/thread counts while
an initialized subscription is waiting; they are diagnostic host figures, not
an MCU RAM claim.

The reference trace separates two surfaces:

- `loader-symbols.txt` is the dynamic `rmw_implementation` ABI;
- `runtime-symbols.txt` contains functions actually called by this minimal
  application;
- `missing-loader-symbols.txt` must be empty. Exporting a symbol does not imply
  semantic support; the supported profile above remains authoritative.

The physical ESP32 Phase 2 acceptance test remains pending. Phase 3 was started
by explicit user direction while that network-level hardware gap is retained
in the project status.
