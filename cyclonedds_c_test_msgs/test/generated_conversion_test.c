// SPDX-License-Identifier: Apache-2.0

#define _POSIX_C_SOURCE 200809L

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <rcutils/error_handling.h>
#include <rosidl_runtime_c/message_type_support_struct.h>
#include <rosidl_typesupport_c/message_type_support_dispatch.h>

#include "cyclonedds_c_test_msgs/msg/NestedFixed_.h"
#include "cyclonedds_c_test_msgs/msg/detail/nested_fixed__rosidl_typesupport_cyclonedds_c.h"
#include "cyclonedds_c_test_msgs/msg/detail/nested_fixed__struct.h"
#include "cyclonedds_c_test_msgs/msg/detail/nested_fixed__type_support.h"
#include "rosidl_typesupport_cyclonedds_c/identifier.h"
#include "rosidl_typesupport_cyclonedds_c/message_type_support.h"

enum { conversion_iterations = 1000000 };

static uint64_t elapsed_ns(const struct timespec *start, const struct timespec *end)
{
  const uint64_t seconds = (uint64_t)(end->tv_sec - start->tv_sec);
  const int64_t nanoseconds = end->tv_nsec - start->tv_nsec;
  return (seconds * UINT64_C(1000000000)) + (uint64_t)nanoseconds;
}

int main(void)
{
  const rosidl_message_type_support_t *generic =
      ROSIDL_GET_MSG_TYPE_SUPPORT(cyclonedds_c_test_msgs, msg, NestedFixed);
  const rosidl_message_type_support_t *support =
      rosidl_typesupport_c__get_message_typesupport_handle_function(
          generic, rosidl_typesupport_cyclonedds_c__identifier);
  if (support == NULL || support->data == NULL) {
    fprintf(stderr, "custom type-support dispatch failed: %s\n", rcutils_get_error_string().str);
    return 1;
  }
  const rosidl_typesupport_cyclonedds_c__message_type_support_callbacks_t *callbacks =
      support->data;
  if (strcmp(callbacks->ros_type_name, "cyclonedds_c_test_msgs/msg/NestedFixed") != 0 ||
      strcmp(callbacks->dds_type_name, "cyclonedds_c_test_msgs::msg::dds_::NestedFixed_") != 0 ||
      callbacks->descriptor == NULL ||
      callbacks->ros_size != sizeof(cyclonedds_c_test_msgs__msg__NestedFixed) ||
      callbacks->dds_size != sizeof(cyclonedds_c_test_msgs_msg_dds__NestedFixed_)) {
    fprintf(stderr, "generated callback metadata mismatch\n");
    return 2;
  }

  cyclonedds_c_test_msgs__msg__NestedFixed ros_sample = {0};
  cyclonedds_c_test_msgs_msg_dds__NestedFixed_ dds_sample = {0};
  cyclonedds_c_test_msgs__msg__NestedFixed roundtrip = {0};
  ros_sample.counter.data = UINT32_C(42424242);
  for (size_t index = 0U; index < 4U; ++index) {
    ros_sample.samples.values[index] = UINT32_C(1000) + (uint32_t)index;
  }

  if (!callbacks->ros_to_dds(&ros_sample, &dds_sample) ||
      !callbacks->dds_to_ros(&dds_sample, &roundtrip) ||
      roundtrip.counter.data != ros_sample.counter.data ||
      memcmp(roundtrip.samples.values, ros_sample.samples.values,
             sizeof(ros_sample.samples.values)) != 0) {
    fprintf(stderr, "generated nested conversion failed\n");
    return 3;
  }

  struct timespec start;
  struct timespec end;
  if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {
    return 4;
  }
  for (size_t iteration = 0U; iteration < conversion_iterations; ++iteration) {
    ros_sample.counter.data += UINT32_C(1);
    if (!callbacks->ros_to_dds(&ros_sample, &dds_sample) ||
        !callbacks->dds_to_ros(&dds_sample, &roundtrip)) {
      return 5;
    }
  }
  if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
    return 6;
  }
  const uint64_t total_ns = elapsed_ns(&start, &end);
  printf("RMW_CYCLONEDDS_C_GENERATOR_PASS iterations=%u total_ns=%" PRIu64 " roundtrip_ns=%" PRIu64
         " ros_size=%zu dds_size=%zu value=%" PRIu32 "\n",
         conversion_iterations, total_ns, total_ns / conversion_iterations, callbacks->ros_size,
         callbacks->dds_size, roundtrip.counter.data);
  return 0;
}
