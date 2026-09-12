# Cyclone DDS C ROSIDL type support

This experimental Kilted generator turns ROS IDL into fixed-size Cyclone DDS C
type support. It deliberately composes existing tools:

1. `rosidl_generator_dds_idl` produces the official ROS-to-DDS IDL mapping.
2. Cyclone `idlc_generate()` produces DDS C types, descriptors, and serializer
   metadata.
3. This package generates explicit ROS C↔DDS C field converters and a standard
   `rosidl_message_type_support_t` handle.

The accepted Phase 4 profile supports basic scalar members, fixed arrays of
basic or nested message types, and nested fixed-size messages. Strings,
sequences, and other variable-size members fail at generation time. Services
and actions are not generated.

Build and test it from the lab root with:

```sh
scripts/phase4.sh all
```

The package is registered in the `rosidl_typesupport_c` resource index, so an
interface package that finds it before `rosidl_generate_interfaces()` receives
the custom target automatically. It is validated in the pinned Linux container
and is cross-compiled as part of the Phase 5 Zephyr `native_sim` and ESP32
builds. The native Zephyr loopback has executed; the ESP32 result is currently
compile/link-only.

## Repository ownership

This generator is middleware-specific but not Zephyr-specific. It should not
become an internal directory of `ros2_zephyr`. During incubation and the first
public releases it should live beside `rmw_cyclonedds_c` in one standalone
middleware repository, because the two packages share a fixed-type profile,
tests, and version contract. A separate repository becomes useful only after
the generator has independent consumers or an independent upstream/release
owner. See
[`docs/architecture-and-repositories.md`](../../docs/architecture-and-repositories.md).
