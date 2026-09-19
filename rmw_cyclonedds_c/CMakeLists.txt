# SPDX-License-Identifier: Apache-2.0

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
find_package(std_msgs REQUIRED)

set(RMW_CYCLONEDDS_C_DEFAULT_DOMAIN_ID
    ""
    CACHE STRING "Domain ID used when ROS_DOMAIN_ID is unavailable"
)
set(RMW_CYCLONEDDS_C_URI
    ""
    CACHE STRING "Cyclone DDS XML used when CYCLONEDDS_URI is unavailable"
)
set(RMW_CYCLONEDDS_C_GRAPH_MAX_NODES
    8
    CACHE STRING "Maximum number of local nodes in one graph announcement"
)
set(RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE
    16
    CACHE STRING "Maximum local publishers or subscriptions associated with one node"
)
set(RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_PARTICIPANTS
    8
    CACHE STRING "Maximum participants retained by the graph cache"
)
set(RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_NODES
    16
    CACHE STRING "Maximum nodes retained by the graph cache"
)
set(RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_ENDPOINTS
    32
    CACHE STRING "Maximum DDS endpoints retained by the graph cache"
)
foreach(graph_limit RMW_CYCLONEDDS_C_GRAPH_MAX_NODES
                    RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE
                    RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_PARTICIPANTS
                    RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_NODES
                    RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_ENDPOINTS)
  if(NOT "${${graph_limit}}" MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR "${graph_limit} must be a positive integer")
  endif()
endforeach()

idlc_generate(
  TARGET uint32_dds_type_support FILES "${CMAKE_CURRENT_SOURCE_DIR}/idl/UInt32_.idl" WARNINGS
  no-implicit-extensibility
)
idlc_generate(
  TARGET graph_dds_type_support
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/idl/ParticipantEntitiesInfo_.idl"
  WARNINGS no-implicit-extensibility
)

set(_rmw_library_type SHARED)
if(ROS2_ZEPHYR_STATIC_BUILD)
  set(_rmw_library_type STATIC)
endif()
add_library(${PROJECT_NAME} ${_rmw_library_type} src/rmw_cyclonedds_c.c src/rmw_unsupported.c)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wconversion -Werror)
target_compile_definitions(
  ${PROJECT_NAME}
  PRIVATE RMW_CYCLONEDDS_C_GRAPH_MAX_NODES=${RMW_CYCLONEDDS_C_GRAPH_MAX_NODES}
          RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE=${RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE}
          RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_PARTICIPANTS=${RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_PARTICIPANTS}
          RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_NODES=${RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_NODES}
          RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_ENDPOINTS=${RMW_CYCLONEDDS_C_GRAPH_CACHE_MAX_ENDPOINTS}
)
if(NOT RMW_CYCLONEDDS_C_DEFAULT_DOMAIN_ID STREQUAL "")
  target_compile_definitions(
    ${PROJECT_NAME}
    PRIVATE RMW_CYCLONEDDS_C_DEFAULT_DOMAIN_ID=${RMW_CYCLONEDDS_C_DEFAULT_DOMAIN_ID}
  )
endif()
if(NOT RMW_CYCLONEDDS_C_URI STREQUAL "")
  target_compile_definitions(${PROJECT_NAME} PRIVATE RMW_CYCLONEDDS_C_URI="${RMW_CYCLONEDDS_C_URI}")
endif()
set_source_files_properties(src/rmw_unsupported.c PROPERTIES COMPILE_OPTIONS -Wno-unused-parameter)
target_link_libraries(
  ${PROJECT_NAME}
  PRIVATE $<BUILD_INTERFACE:uint32_dds_type_support> $<BUILD_INTERFACE:graph_dds_type_support>
          CycloneDDS::ddsc rmw::rmw
          rosidl_runtime_c::rosidl_runtime_c rosidl_typesupport_c::rosidl_typesupport_c
          ${std_msgs_TARGETS}
)
if(rosidl_typesupport_cyclonedds_c_FOUND)
  target_compile_definitions(${PROJECT_NAME} PRIVATE RMW_CYCLONEDDS_C_HAS_GENERATED_TYPESUPPORT=1)
  target_link_libraries(
    ${PROJECT_NAME} PRIVATE rosidl_typesupport_cyclonedds_c::rosidl_typesupport_cyclonedds_c
  )
endif()

if(BUILD_TESTING)
  find_package(ament_cmake_test REQUIRED)
  find_package(cyclonedds_c_test_msgs QUIET)
  find_package(rcl REQUIRED)
  find_package(rclc REQUIRED)
  add_executable(rmw_smoke_test test/trace_minimal_rclc.c)
  add_executable(uint32_interop test/interop_rclc.c)
  add_executable(contract_test test/contract_negative.c)
  add_executable(graph_fixture test/graph_fixture.c)
  add_executable(graph_query_fixture test/graph_query_fixture.c)
  set(_rmw_test_targets rmw_smoke_test uint32_interop contract_test graph_fixture
                        graph_query_fixture
  )
  if(cyclonedds_c_test_msgs_FOUND AND rosidl_typesupport_cyclonedds_c_FOUND)
    add_executable(generated_rmw_test test/generated_rmw.c)
    add_executable(generated_interop test/generated_interop_rclc.c)
    add_executable(reliable_loss test/reliable_loss.c)
    add_executable(transient_local_test test/transient_local.c)
    list(APPEND _rmw_test_targets generated_rmw_test generated_interop reliable_loss
         transient_local_test
    )
  endif()
  foreach(test_target ${_rmw_test_targets})
    target_compile_options(${test_target} PRIVATE -Wall -Wextra -Wconversion -Werror)
    target_link_libraries(${test_target} PRIVATE rcl::rcl rclc::rclc rmw::rmw ${std_msgs_TARGETS})
    if(cyclonedds_c_test_msgs_FOUND)
      target_link_libraries(${test_target} PRIVATE ${cyclonedds_c_test_msgs_TARGETS})
    endif()
  endforeach()
  target_link_libraries(
    graph_query_fixture PRIVATE CycloneDDS::ddsc $<BUILD_INTERFACE:uint32_dds_type_support>
  )

  function(add_rmw_cyclonedds_c_test test_name target_name domain_id)
    ament_add_test(
      ${test_name}
      COMMAND $<TARGET_FILE:${target_name}> ${ARGN}
      ENV RMW_IMPLEMENTATION=rmw_cyclonedds_c ROS_DOMAIN_ID=${domain_id}
      GENERATE_RESULT_FOR_RETURN_CODE_ZERO
      TIMEOUT 30
    )
  endfunction()

  add_rmw_cyclonedds_c_test(rmw_smoke rmw_smoke_test 111)
  add_rmw_cyclonedds_c_test(rmw_contract contract_test 112)
  if(cyclonedds_c_test_msgs_FOUND AND rosidl_typesupport_cyclonedds_c_FOUND)
    add_rmw_cyclonedds_c_test(rmw_generated_best_effort generated_rmw_test 113 best_effort)
    add_rmw_cyclonedds_c_test(rmw_generated_reliable generated_rmw_test 114 reliable)
    add_rmw_cyclonedds_c_test(rmw_transient_local transient_local_test 115)
  endif()

  install(TARGETS ${_rmw_test_targets} DESTINATION lib/${PROJECT_NAME})
endif()
install(
  TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(TARGETS uint32_dds_type_support graph_dds_type_support ARCHIVE DESTINATION lib)

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(CycloneDDS rmw rosidl_runtime_c rosidl_typesupport_c std_msgs)
if(rosidl_typesupport_cyclonedds_c_FOUND)
  ament_export_dependencies(rosidl_typesupport_cyclonedds_c)
endif()
register_rmw_implementation("c:rosidl_typesupport_c")

ament_package()
