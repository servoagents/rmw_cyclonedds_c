# rosidl_typesupport_cyclonedds_c

This ROSIDL extension generates Cyclone DDS C type support for fixed-size ROS
messages:

1. `rosidl_generator_dds_idl` produces the standard ROS-to-DDS IDL mapping.
2. Cyclone `idlc_generate()` produces DDS C types and serializer metadata.
3. This package generates field-by-field ROS C to DDS C conversion functions
   and a standard `rosidl_message_type_support_t` handle.

The current profile supports scalar fields, fixed arrays, and nested
fixed-size messages. Strings, sequences, services, and actions are rejected at
generation time. ROS and DDS structs are never pointer-reinterpreted.

The package registers itself with `rosidl_typesupport_c`, so interface packages
can select it through the normal ROSIDL extension mechanism. See the
[repository README](../README.md) for the tested build workflow.
