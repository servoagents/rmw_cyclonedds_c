# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)
project(cyclonedds_c_test_msgs C CXX)

if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(rcutils REQUIRED)
find_package(rosidl_typesupport_c REQUIRED)
find_package(rosidl_typesupport_cyclonedds_c REQUIRED)

rosidl_generate_interfaces(
  ${PROJECT_NAME} "msg/UInt32.msg" "msg/FixedArray.msg" "msg/NestedFixed.msg"
)

rosidl_get_typesupport_target(_cyclonedds_c_target ${PROJECT_NAME} rosidl_typesupport_cyclonedds_c)
rosidl_get_typesupport_target(_generic_c_target ${PROJECT_NAME} rosidl_typesupport_c)

if(BUILD_TESTING)
  add_executable(generated_conversion_test test/generated_conversion_test.c)
  target_compile_options(generated_conversion_test PRIVATE -Wall -Wextra -Wconversion -Werror)
  target_link_libraries(
    generated_conversion_test
    PRIVATE ${PROJECT_NAME}__rosidl_generator_c ${_generic_c_target} ${_cyclonedds_c_target}
            rcutils::rcutils rosidl_typesupport_c::rosidl_typesupport_c
            rosidl_typesupport_cyclonedds_c::rosidl_typesupport_cyclonedds_c
  )

  install(TARGETS generated_conversion_test DESTINATION lib/${PROJECT_NAME})
endif()

ament_export_dependencies(rosidl_default_runtime)
ament_package()
