# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20)
project(rosidl_typesupport_cyclonedds_c C)

if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
find_package(CycloneDDS REQUIRED)
find_package(rosidl_runtime_c REQUIRED)

ament_python_install_package(${PROJECT_NAME})

set(_typesupport_library_type SHARED)
if(ROS2_ZEPHYR_STATIC_BUILD)
  set(_typesupport_library_type STATIC)
endif()
add_library(${PROJECT_NAME} ${_typesupport_library_type} src/identifier.c)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wconversion -Werror)
target_include_directories(
  ${PROJECT_NAME} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                         "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(${PROJECT_NAME} PUBLIC CycloneDDS::ddsc rosidl_runtime_c::rosidl_runtime_c)

install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY cmake resource DESTINATION share/${PROJECT_NAME})
install(PROGRAMS scripts/check_fixed_profile.py scripts/rosidl_typesupport_cyclonedds_c
        DESTINATION lib/${PROJECT_NAME}
)
install(
  TARGETS ${PROJECT_NAME}
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

ament_export_targets(export_${PROJECT_NAME} HAS_LIBRARY_TARGET)
ament_export_dependencies(CycloneDDS rosidl_runtime_c)
ament_index_register_resource("rosidl_typesupport_c")
ament_package(CONFIG_EXTRAS "cmake/rosidl_typesupport_cyclonedds_c-extras.cmake")
