FROM ros:kilted-ros-base-noble@sha256:0030f32dc8a71ef8401c89470db6003c779f036f532d40195790f58f0001902d

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
      ltrace \
      ros-kilted-cyclonedds \
      ros-kilted-rclc \
      ros-kilted-rmw-cyclonedds-cpp \
      ros-kilted-rosidl-generator-dds-idl \
      ros-kilted-ros2topic \
      shellcheck \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY cyclonedds_c_test_msgs src/cyclonedds_c_test_msgs
COPY rmw_cyclonedds_c src/rmw_cyclonedds_c
COPY rosidl_typesupport_cyclonedds_c src/rosidl_typesupport_cyclonedds_c
COPY scripts src/scripts

RUN shellcheck src/scripts/*.sh

RUN source /opt/ros/kilted/setup.sh \
    && colcon --log-base log build \
      --build-base build \
      --install-base install \
      --packages-up-to rmw_cyclonedds_c \
      --cmake-args -DCMAKE_BUILD_TYPE=RelWithDebInfo

RUN source /opt/ros/kilted/setup.sh \
    && source install/setup.sh \
    && install/rosidl_typesupport_cyclonedds_c/lib/rosidl_typesupport_cyclonedds_c/check_fixed_profile.py \
    && install/cyclonedds_c_test_msgs/lib/cyclonedds_c_test_msgs/generated_conversion_test \
    && RMW_IMPLEMENTATION=rmw_cyclonedds_c \
      install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/generated_rmw_test \
    && RMW_IMPLEMENTATION=rmw_cyclonedds_c \
      install/rmw_cyclonedds_c/lib/rmw_cyclonedds_c/rmw_smoke_test
