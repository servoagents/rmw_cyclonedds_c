# SPDX-License-Identifier: Apache-2.0

if(DEFINED ROSIDL_TYPESUPPORT_CYCLONEDDS_C_GENERATE_PACKAGES
   AND NOT ROSIDL_TYPESUPPORT_CYCLONEDDS_C_GENERATE_PACKAGES STREQUAL ""
)
  list(FIND ROSIDL_TYPESUPPORT_CYCLONEDDS_C_GENERATE_PACKAGES "${PROJECT_NAME}"
       _cyclonedds_package_index
  )
  if(_cyclonedds_package_index EQUAL -1)
    return()
  endif()
endif()

if(NOT TARGET ${rosidl_generate_interfaces_TARGET}__rosidl_generator_c)
  message(
    FATAL_ERROR "The rosidl_generator_c extension must run before rosidl_typesupport_cyclonedds_c"
  )
endif()

find_package(CycloneDDS REQUIRED)
find_package(rosidl_generator_dds_idl REQUIRED)
find_package(rosidl_runtime_c REQUIRED)
find_package(rosidl_typesupport_interface REQUIRED)

set(_dds_idl_target "${rosidl_generate_interfaces_TARGET}__rosidl_typesupport_cyclonedds_c_dds_idl")
rosidl_generate_dds_interfaces(
  ${_dds_idl_target} IDL_TUPLES "${rosidl_generate_interfaces_IDL_TUPLES}" DEPENDENCY_PACKAGE_NAMES
  "${rosidl_generate_interfaces_DEPENDENCY_PACKAGE_NAMES}"
)

set(_dds_output_base "${CMAKE_CURRENT_BINARY_DIR}/rosidl_generator_dds_idl/${PROJECT_NAME}")
file(MAKE_DIRECTORY "${_dds_output_base}")
set(_dds_idl_files "")
set(_generated_files "")
set(_output_path "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_cyclonedds_c/${PROJECT_NAME}")
foreach(_abs_idl_file ${rosidl_generate_interfaces_ABS_IDL_FILES})
  get_filename_component(_parent_folder "${_abs_idl_file}" DIRECTORY)
  get_filename_component(_parent_folder "${_parent_folder}" NAME)
  get_filename_component(_idl_name "${_abs_idl_file}" NAME_WE)
  string_camel_case_to_lower_case_underscore("${_idl_name}" _header_name)
  list(APPEND _dds_idl_files "${_dds_output_base}/${_parent_folder}/${_idl_name}_.idl")
  list(APPEND _generated_files
       "${_output_path}/${_parent_folder}/detail/${_header_name}__rosidl_typesupport_cyclonedds_c.h"
       "${_output_path}/${_parent_folder}/detail/${_header_name}__type_support_c.c"
  )
endforeach()

set(_cyclonedds_idl_target
    "${rosidl_generate_interfaces_TARGET}__rosidl_typesupport_cyclonedds_c_idlc"
)
idlc_generate(
  TARGET
  ${_cyclonedds_idl_target}
  FILES
  ${_dds_idl_files}
  BASE_DIR
  "${CMAKE_CURRENT_BINARY_DIR}/rosidl_generator_dds_idl"
  INCLUDES
  "${CMAKE_CURRENT_BINARY_DIR}/rosidl_generator_dds_idl"
  FEATURES
  case-sensitive
  WARNINGS
  no-implicit-extensibility
  DEPENDS
  ${_dds_idl_target}
)

set(_dependency_files "")
set(_dependencies "")
foreach(_pkg_name ${rosidl_generate_interfaces_DEPENDENCY_PACKAGE_NAMES})
  foreach(_idl_file ${${_pkg_name}_IDL_FILES})
    set(_abs_idl_file "${${_pkg_name}_DIR}/../${_idl_file}")
    normalize_path(_abs_idl_file "${_abs_idl_file}")
    list(APPEND _dependency_files "${_abs_idl_file}")
    list(APPEND _dependencies "${_pkg_name}:${_abs_idl_file}")
  endforeach()
endforeach()

set(_generator_arguments_file
    "${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_cyclonedds_c__arguments.json"
)
set(_target_dependencies
    "${rosidl_typesupport_cyclonedds_c_BIN}"
    ${rosidl_typesupport_cyclonedds_c_GENERATOR_FILES}
    "${rosidl_typesupport_cyclonedds_c_TEMPLATE_DIR}/idl__rosidl_typesupport_cyclonedds_c.h.em"
    "${rosidl_typesupport_cyclonedds_c_TEMPLATE_DIR}/idl__type_support_c.c.em"
    ${rosidl_generate_interfaces_ABS_IDL_FILES}
    ${_dependency_files}
)
rosidl_write_generator_arguments(
  "${_generator_arguments_file}"
  PACKAGE_NAME
  "${PROJECT_NAME}"
  IDL_TUPLES
  "${rosidl_generate_interfaces_IDL_TUPLES}"
  ROS_INTERFACE_DEPENDENCIES
  "${_dependencies}"
  OUTPUT_DIR
  "${_output_path}"
  TEMPLATE_DIR
  "${rosidl_typesupport_cyclonedds_c_TEMPLATE_DIR}"
  TARGET_DEPENDENCIES
  ${_target_dependencies}
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(
  OUTPUT ${_generated_files}
  COMMAND Python3::Interpreter ARGS "${rosidl_typesupport_cyclonedds_c_BIN}"
          --generator-arguments-file "${_generator_arguments_file}"
  DEPENDS ${_target_dependencies}
  COMMENT "Generating fixed-size Cyclone DDS C type support"
  VERBATIM
)

set(_target_suffix "__rosidl_typesupport_cyclonedds_c")
set(_generated_typesupport_library_type SHARED)
if(ROS2_ZEPHYR_STATIC_BUILD)
  set(_generated_typesupport_library_type STATIC)
endif()
add_library(
  ${rosidl_generate_interfaces_TARGET}${_target_suffix} ${_generated_typesupport_library_type}
                                                        ${_generated_files}
)
target_compile_options(
  ${rosidl_generate_interfaces_TARGET}${_target_suffix} PRIVATE -Wall -Wextra -Wconversion -Werror
)
target_include_directories(
  ${rosidl_generate_interfaces_TARGET}${_target_suffix}
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/rosidl_typesupport_cyclonedds_c>"
         "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
         "$<INSTALL_INTERFACE:include/${PROJECT_NAME}>"
)
target_link_libraries(
  ${rosidl_generate_interfaces_TARGET}${_target_suffix}
  PRIVATE ${rosidl_generate_interfaces_TARGET}__rosidl_generator_c
          $<BUILD_INTERFACE:${_cyclonedds_idl_target}>
          CycloneDDS::ddsc
          rosidl_runtime_c::rosidl_runtime_c
          rosidl_typesupport_cyclonedds_c::rosidl_typesupport_cyclonedds_c
)

foreach(_pkg_name ${rosidl_generate_interfaces_DEPENDENCY_PACKAGE_NAMES})
  if(TARGET ${${_pkg_name}_TARGETS${_target_suffix}})
    target_link_libraries(
      ${rosidl_generate_interfaces_TARGET}${_target_suffix}
      PRIVATE ${${_pkg_name}_TARGETS${_target_suffix}}
    )
  endif()
endforeach()

add_dependencies(
  ${rosidl_generate_interfaces_TARGET} ${rosidl_generate_interfaces_TARGET}${_target_suffix}
)

if(NOT rosidl_generate_interfaces_SKIP_INSTALL)
  install(
    DIRECTORY "${_output_path}/"
    DESTINATION "include/${PROJECT_NAME}/${PROJECT_NAME}"
    PATTERN "*.c" EXCLUDE
  )
  ament_export_include_directories("include/${PROJECT_NAME}")
  rosidl_export_typesupport_libraries(
    ${_target_suffix} ${rosidl_generate_interfaces_TARGET}${_target_suffix}
  )
  ament_export_targets(export_${rosidl_generate_interfaces_TARGET}${_target_suffix})
  rosidl_export_typesupport_targets(
    ${_target_suffix} ${rosidl_generate_interfaces_TARGET}${_target_suffix}
  )
  install(
    TARGETS ${rosidl_generate_interfaces_TARGET}${_target_suffix}
    EXPORT export_${rosidl_generate_interfaces_TARGET}${_target_suffix}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
  )
  install(TARGETS ${_cyclonedds_idl_target} ARCHIVE DESTINATION lib)
  ament_export_dependencies(CycloneDDS rosidl_runtime_c rosidl_typesupport_cyclonedds_c)
endif()
