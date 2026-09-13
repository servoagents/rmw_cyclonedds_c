ARG ROS_BASE_IMAGE=ros:lyrical-ros-base-resolute@sha256:dbb2a254523ee3c40ec9fc07956bc1042253c7beb3b6dad4a4585d85c99e9716
FROM ${ROS_BASE_IMAGE}

ARG ROS_DISTRO=lyrical
ENV ROS_DISTRO=${ROS_DISTRO}

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
      ltrace \
      ros-${ROS_DISTRO}-cyclonedds \
      ros-${ROS_DISTRO}-rclc \
      ros-${ROS_DISTRO}-rmw-cyclonedds-cpp \
      ros-${ROS_DISTRO}-rosidl-generator-dds-idl \
      ros-${ROS_DISTRO}-ros2topic \
      shellcheck \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY cyclonedds_c_test_msgs src/cyclonedds_c_test_msgs
COPY rmw_cyclonedds_c src/rmw_cyclonedds_c
COPY rosidl_typesupport_cyclonedds_c src/rosidl_typesupport_cyclonedds_c
COPY scripts src/scripts

RUN shellcheck src/scripts/*.sh

RUN source "/opt/ros/${ROS_DISTRO}/setup.sh" \
    && colcon --log-base log build \
      --executor sequential \
      --build-base build \
      --install-base install \
      --packages-up-to rmw_cyclonedds_c \
      --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo

RUN source "/opt/ros/${ROS_DISTRO}/setup.sh" \
    && source install/setup.sh \
    && install/rosidl_typesupport_cyclonedds_c/lib/rosidl_typesupport_cyclonedds_c/check_fixed_profile.py \
    && install/cyclonedds_c_test_msgs/lib/cyclonedds_c_test_msgs/generated_conversion_test \
    && RMW_IMPLEMENTATION=rmw_cyclonedds_c \
      install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/generated_rmw_test \
    && RMW_IMPLEMENTATION=rmw_cyclonedds_c \
      install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/rmw_smoke_test
