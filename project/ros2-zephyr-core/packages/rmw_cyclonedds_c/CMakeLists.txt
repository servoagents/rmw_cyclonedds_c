cmake_minimum_required(VERSION 3.20)
project(rmw_cyclonedds_c C)

if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(ament_cmake REQUIRED)
find_package(CycloneDDS REQUIRED)
find_package(rmw REQUIRED)
find_package(rosidl_runtime_c REQUIRED)
find_package(rosidl_typesupport_c REQUIRED)
find_package(rosidl_typesupport_cyclonedds_c QUIET)
find_package(phase4_test_msgs QUIET)
find_package(std_msgs REQUIRED)

set(RMW_CYCLONEDDS_C_DEFAULT_DOMAIN_ID "" CACHE STRING
  "Domain ID used when ROS_DOMAIN_ID is unavailable")
set(RMW_CYCLONEDDS_C_URI "" CACHE STRING
  "Cyclone DDS XML used when CYCLONEDDS_URI is unavailable")

idlc_generate(
  TARGET phase3_uint32_dds_type
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/idl/UInt32_.idl"
  WARNINGS no-implicit-extensibility)

set(_rmw_library_type SHARED)
if(ROS2_ZEPHYR_STATIC_BUILD)
  set(_rmw_library_type STATIC)
endif()
add_library(${PROJECT_NAME} ${_rmw_library_type}
  src/rmw_cyclonedds_c.c
  src/rmw_unsupported.c)
target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall -Wextra -Wconversion -Werror)
if(NOT RMW_CYCLONEDDS_C_DEFAULT_DOMAIN_ID STREQUAL "")
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    RMW_CYCLONEDDS_C_DEFAULT_DOMAIN_ID=${RMW_CYCLONEDDS_C_DEFAULT_DOMAIN_ID})
endif()
if(NOT RMW_CYCLONEDDS_C_URI STREQUAL "")
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    RMW_CYCLONEDDS_C_URI="${RMW_CYCLONEDDS_C_URI}")
endif()
set_source_files_properties(src/rmw_unsupported.c PROPERTIES
  COMPILE_OPTIONS -Wno-unused-parameter)
target_link_libraries(${PROJECT_NAME} PRIVATE
  $<BUILD_INTERFACE:phase3_uint32_dds_type>
  CycloneDDS::ddsc
  rmw::rmw
  rosidl_runtime_c::rosidl_runtime_c
  rosidl_typesupport_c::rosidl_typesupport_c
  ${std_msgs_TARGETS})
if(rosidl_typesupport_cyclonedds_c_FOUND)
  target_compile_definitions(${PROJECT_NAME} PRIVATE
    RMW_CYCLONEDDS_C_HAS_GENERATED_TYPESUPPORT=1)
  target_link_libraries(${PROJECT_NAME} PRIVATE
    rosidl_typesupport_cyclonedds_c::rosidl_typesupport_cyclonedds_c)
endif()

if(BUILD_TESTING)
  find_package(rcl REQUIRED)
  find_package(rclc REQUIRED)
  add_executable(phase3_trace_minimal_rclc
    test/trace_minimal_rclc.c)
  add_executable(phase3_interop_rclc
    test/interop_rclc.c)
  add_executable(phase3_contract_negative
    test/contract_negative.c)
  set(_rmw_test_targets
      phase3_trace_minimal_rclc
      phase3_interop_rclc
      phase3_contract_negative)
  if(phase4_test_msgs_FOUND AND rosidl_typesupport_cyclonedds_c_FOUND)
    add_executable(phase4_generated_rmw
      test/generated_rmw.c)
    add_executable(phase4_generated_interop_rclc
      test/generated_interop_rclc.c)
    list(APPEND _rmw_test_targets
      phase4_generated_rmw phase4_generated_interop_rclc)
  endif()
  foreach(test_target ${_rmw_test_targets})
    target_compile_options(${test_target} PRIVATE
      -Wall -Wextra -Wconversion -Werror)
    target_link_libraries(${test_target} PRIVATE
      rcl::rcl
      rclc::rclc
      rmw::rmw
      ${std_msgs_TARGETS})
    if(phase4_test_msgs_FOUND)
      target_link_libraries(${test_target} PRIVATE ${phase4_test_msgs_TARGETS})
    endif()
  endforeach()

  install(TARGETS ${_rmw_test_targets}
    DESTINATION lib/${PROJECT_NAME})
endif()
install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(TARGETS phase3_uint32_dds_type
  ARCHIVE DESTINATION lib)

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(
  CycloneDDS
  rmw
  rosidl_runtime_c
  rosidl_typesupport_c
  std_msgs)
if(rosidl_typesupport_cyclonedds_c_FOUND)
  ament_export_dependencies(rosidl_typesupport_cyclonedds_c)
endif()
register_rmw_implementation("c:rosidl_typesupport_c")

ament_package()
