ARG ROS_BASE_IMAGE=ros:lyrical-ros-base-resolute@sha256:dbb2a254523ee3c40ec9fc07956bc1042253c7beb3b6dad4a4585d85c99e9716
FROM ${ROS_BASE_IMAGE}

ARG ROS_DISTRO=lyrical
ARG TEST_RMW_IMPLEMENTATION_REV=1a9b0e672a787af8c3a740d21dece9389d6b77ea
ENV ROS_DISTRO=${ROS_DISTRO}

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
      ltrace \
      iproute2 \
      git \
      ros-${ROS_DISTRO}-ament-lint-common \
      ros-${ROS_DISTRO}-cyclonedds \
      ros-${ROS_DISTRO}-osrf-testing-tools-cpp \
      ros-${ROS_DISTRO}-rclc \
      ros-${ROS_DISTRO}-rmw-dds-common \
      ros-${ROS_DISTRO}-rmw-cyclonedds-cpp \
      ros-${ROS_DISTRO}-rosidl-generator-dds-idl \
      ros-${ROS_DISTRO}-ros2topic \
      ros-${ROS_DISTRO}-test-msgs \
      shellcheck \
    && rm -rf /var/lib/apt/lists/*

RUN git init /tmp/rmw_implementation \
    && git -C /tmp/rmw_implementation remote add origin \
      https://github.com/ros2/rmw_implementation.git \
    && git -C /tmp/rmw_implementation fetch --depth 1 origin "${TEST_RMW_IMPLEMENTATION_REV}" \
    && git -C /tmp/rmw_implementation checkout --detach FETCH_HEAD \
    && cp -a /tmp/rmw_implementation/test_rmw_implementation /opt/test_rmw_implementation \
    && rm -rf /tmp/rmw_implementation

WORKDIR /workspace
COPY cyclonedds_c_test_msgs src/cyclonedds_c_test_msgs
COPY rmw_cyclonedds_c src/rmw_cyclonedds_c
COPY rosidl_typesupport_cyclonedds_c src/rosidl_typesupport_cyclonedds_c
COPY scripts src/scripts

RUN shellcheck src/scripts/*.sh

RUN source "/opt/ros/${ROS_DISTRO}/setup.sh" \
    && CMAKE_BUILD_PARALLEL_LEVEL=1 colcon --log-base log build \
      --executor sequential \
      --build-base build \
      --install-base install \
      --packages-up-to rmw_cyclonedds_c \
      --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo

RUN source "/opt/ros/${ROS_DISTRO}/setup.sh" \
    && source install/setup.sh \
    && install/rosidl_typesupport_cyclonedds_c/lib/rosidl_typesupport_cyclonedds_c/check_fixed_profile.py \
    && install/cyclonedds_c_test_msgs/lib/cyclonedds_c_test_msgs/generated_conversion_test \
    && CMAKE_BUILD_PARALLEL_LEVEL=1 colcon --log-base log-test test \
      --build-base build \
      --install-base install \
      --packages-select rmw_cyclonedds_c \
      --event-handlers console_direct+ \
    && colcon test-result --test-result-base build/rmw_cyclonedds_c --verbose
