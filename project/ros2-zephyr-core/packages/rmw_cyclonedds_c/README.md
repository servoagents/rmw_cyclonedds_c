# Experimental Cyclone DDS C RMW

This package began as the Phase 3 Linux-first vertical slice and now also
accepts Phase 4 generated fixed-size type support. It is intentionally not a
complete RMW implementation.

The initial supported profile is:

- ordinary `rclc`/`rcl` initialization and cleanup;
- one shared Cyclone DDS participant per RMW context;
- nodes and their required graph guard condition;
- generated scalar, fixed-array, and nested fixed-size publishers and
  subscriptions when `rosidl_typesupport_cyclonedds_c` is installed;
- `std_msgs/msg/UInt32` through the original Phase 3 compatibility adapter;
- ROS topic naming (`rt/<absolute-topic>`);
- best-effort, volatile, keep-last QoS;
- publish, take-with-info, and DDS-backed waitsets;
- application guard-condition wakeup, GIDs, and endpoint match counts;
- canonical ROS type-hash metadata in DDS endpoint `USER_DATA`;
- one preallocated DDS workspace per endpoint, protected for concurrent use;
- no RMW-created callback thread and no extra message queue.

Other message types, reliable/transient-local QoS, content filters, loaning,
services, clients, events, and remote graph queries are outside this slice.
Creation calls reject unsupported types and policies. The package must not be
presented as a general-purpose RMW. Kilted resolves its complete dynamic RMW
dispatch table, so the library exports that ABI; operations outside this
profile return `RMW_RET_UNSUPPORTED` (or `NULL`/`false` for their API shape)
with no fake success.

Variable-size strings and sequences are deliberately rejected by the Phase 4
generator. ROS and DDS structs are never pointer-reinterpreted: generated
functions copy each field and recursively invoke generated nested converters.

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

## Generated type-support workflow

From the lab root:

```sh
scripts/phase4.sh all
```

The Phase 4 image builds `rosidl_typesupport_cyclonedds_c` before the fixture
interfaces, allowing the ordinary ROSIDL extension and dispatcher mechanisms
to select it. The lane benchmarks generated conversions, runs a nested message
through `rclc` and this RMW, retains the Phase 3 loopback regression, and tests
the nested message against stock `rmw_cyclonedds_cpp` in both directions.

This remains a Linux build. Zephyr module integration is the next boundary.
