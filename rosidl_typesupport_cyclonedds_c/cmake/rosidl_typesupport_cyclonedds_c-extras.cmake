# SPDX-License-Identifier: Apache-2.0

find_package(CycloneDDS REQUIRED)
find_package(ament_cmake_core REQUIRED)
find_package(rosidl_generator_c REQUIRED)
find_package(rosidl_generator_dds_idl REQUIRED)

ament_register_extension(
  "rosidl_generate_idl_interfaces" "rosidl_typesupport_cyclonedds_c"
  "rosidl_typesupport_cyclonedds_c_generate_interfaces.cmake"
)

set(rosidl_typesupport_cyclonedds_c_BIN
    "${rosidl_typesupport_cyclonedds_c_DIR}/../../../lib/rosidl_typesupport_cyclonedds_c/rosidl_typesupport_cyclonedds_c"
)
normalize_path(rosidl_typesupport_cyclonedds_c_BIN "${rosidl_typesupport_cyclonedds_c_BIN}")

file(
  GLOB
  rosidl_typesupport_cyclonedds_c_GENERATOR_FILES
  "${rosidl_typesupport_cyclonedds_c_DIR}/../../../lib/python*/site-packages/rosidl_typesupport_cyclonedds_c/__init__.py"
)
list(LENGTH rosidl_typesupport_cyclonedds_c_GENERATOR_FILES _generator_file_count)
if(NOT _generator_file_count EQUAL 1)
  message(
    FATAL_ERROR
      "Expected one installed rosidl_typesupport_cyclonedds_c generator, found ${_generator_file_count}"
  )
endif()
normalize_path(
  rosidl_typesupport_cyclonedds_c_GENERATOR_FILES
  "${rosidl_typesupport_cyclonedds_c_GENERATOR_FILES}"
)

set(rosidl_typesupport_cyclonedds_c_TEMPLATE_DIR
    "${rosidl_typesupport_cyclonedds_c_DIR}/../resource"
)
normalize_path(
  rosidl_typesupport_cyclonedds_c_TEMPLATE_DIR "${rosidl_typesupport_cyclonedds_c_TEMPLATE_DIR}"
)
