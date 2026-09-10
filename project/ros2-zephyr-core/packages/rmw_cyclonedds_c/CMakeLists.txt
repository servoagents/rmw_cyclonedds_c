cmake_minimum_required(VERSION 3.20)
project(rmw_cyclonedds_c C)

if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(ament_cmake REQUIRED)
find_package(CycloneDDS REQUIRED)
find_package(rcl REQUIRED)
find_package(rclc REQUIRED)
find_package(rmw REQUIRED)
find_package(rosidl_runtime_c REQUIRED)
find_package(std_msgs REQUIRED)

idlc_generate(
  TARGET phase3_uint32_dds_type
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/idl/UInt32_.idl"
  WARNINGS no-implicit-extensibility)

add_library(${PROJECT_NAME} SHARED
  src/rmw_cyclonedds_c.c
  src/rmw_unsupported.c)
target_compile_options(${PROJECT_NAME} PRIVATE
  -Wall -Wextra -Wconversion -Werror)
set_source_files_properties(src/rmw_unsupported.c PROPERTIES
  COMPILE_OPTIONS -Wno-unused-parameter)
target_link_libraries(${PROJECT_NAME} PRIVATE
  phase3_uint32_dds_type
  CycloneDDS::ddsc
  rmw::rmw
  rosidl_runtime_c::rosidl_runtime_c
  ${std_msgs_TARGETS})

add_executable(phase3_trace_minimal_rclc
  test/trace_minimal_rclc.c)
add_executable(phase3_interop_rclc
  test/interop_rclc.c)
add_executable(phase3_contract_negative
  test/contract_negative.c)
foreach(test_target
    phase3_trace_minimal_rclc
    phase3_interop_rclc
    phase3_contract_negative)
  target_compile_options(${test_target} PRIVATE
    -Wall -Wextra -Wconversion -Werror)
  target_link_libraries(${test_target} PRIVATE
    rcl::rcl
    rclc::rclc
    rmw::rmw
    ${std_msgs_TARGETS})
endforeach()

install(TARGETS
  phase3_trace_minimal_rclc
  phase3_interop_rclc
  phase3_contract_negative
  DESTINATION lib/${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(rmw)
register_rmw_implementation("c:rosidl_typesupport_c")

ament_package()
