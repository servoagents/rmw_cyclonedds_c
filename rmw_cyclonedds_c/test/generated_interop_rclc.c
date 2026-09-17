// SPDX-License-Identifier: Apache-2.0

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <cyclonedds_c_test_msgs/msg/nested_fixed.h>
#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <rmw/qos_profiles.h>

enum { RMW_TO_ROS_VALUE = 27182818U, ROS_TO_RMW_VALUE = 31415926U, MAX_WAITS = 300 };

static bool received;
static cyclonedds_c_test_msgs__msg__NestedFixed received_message;

static void receive_nested(const void *untyped_message)
{
  const cyclonedds_c_test_msgs__msg__NestedFixed *message = untyped_message;
  if (message != NULL) {
    received_message = *message;
    received = true;
  }
}

static bool pause_100_ms(void)
{
  const struct timespec delay = {.tv_sec = 0, .tv_nsec = 100000000L};
  return nanosleep(&delay, NULL) == 0;
}

static void cleanup_result(rcl_ret_t cleanup, const char *operation, int *result)
{
  if (cleanup != RCL_RET_OK) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_CLEANUP_ERROR operation=%s rcl_ret=%d\n", operation, cleanup);
    *result = 1;
  }
}

static int run_publisher(rcl_node_t *node, const rmw_qos_profile_t *qos,
                         const char *reliability_name)
{
  int result = 1;
  rcl_publisher_t publisher = rcl_get_zero_initialized_publisher();
  if (rclc_publisher_init(
          &publisher, node, ROSIDL_GET_MSG_TYPE_SUPPORT(cyclonedds_c_test_msgs, msg, NestedFixed),
          "generated_rmw_to_ros", qos) != RCL_RET_OK) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=publisher_init\n");
    return result;
  }
  const rmw_qos_profile_t *actual_qos = rcl_publisher_get_actual_qos(&publisher);
  if (actual_qos == NULL || actual_qos->reliability != qos->reliability) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=publisher_actual_qos\n");
    goto cleanup;
  }
  size_t matched = 0U;
  for (unsigned int wait = 0U; wait < MAX_WAITS && matched == 0U; ++wait) {
    if (rcl_publisher_get_subscription_count(&publisher, &matched) != RCL_RET_OK ||
        !pause_100_ms()) {
      fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=wait_for_subscription\n");
      goto cleanup;
    }
  }
  if (matched == 0U) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_TIMEOUT direction=rmw_to_ros stage=match\n");
    goto cleanup;
  }
  cyclonedds_c_test_msgs__msg__NestedFixed message = {0};
  message.counter.data = RMW_TO_ROS_VALUE;
  for (size_t index = 0U; index < 4U; ++index) {
    message.samples.values[index] = UINT32_C(31) + (uint32_t)index;
  }
  for (unsigned int sequence = 1U; sequence <= 5U; ++sequence) {
    if (rcl_publish(&publisher, &message, NULL) != RCL_RET_OK) {
      fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=publish\n");
      goto cleanup;
    }
    printf("RMW_SENT direction=rmw_to_ros reliability=%s value=%u sequence=%u\n",
           reliability_name, message.counter.data, sequence);
    fflush(stdout);
    if (!pause_100_ms()) {
      goto cleanup;
    }
  }
  result = 0;

cleanup:
  cleanup_result(rcl_publisher_fini(&publisher, node), "publisher_fini", &result);
  return result;
}

static int run_subscription(rcl_node_t *node, rclc_support_t *support, rcl_allocator_t *allocator,
                            const rmw_qos_profile_t *qos, const char *reliability_name)
{
  int result = 1;
  rcl_subscription_t subscription = rcl_get_zero_initialized_subscription();
  rclc_executor_t executor = rclc_executor_get_zero_initialized_executor();
  cyclonedds_c_test_msgs__msg__NestedFixed message = {0};
  if (rclc_subscription_init(
          &subscription, node,
          ROSIDL_GET_MSG_TYPE_SUPPORT(cyclonedds_c_test_msgs, msg, NestedFixed),
          "ros_to_generated_rmw", qos) != RCL_RET_OK) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=subscription_init\n");
    return result;
  }
  const rmw_qos_profile_t *actual_qos = rcl_subscription_get_actual_qos(&subscription);
  if (actual_qos == NULL || actual_qos->reliability != qos->reliability) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=subscription_actual_qos\n");
    goto cleanup_subscription;
  }
  if (rclc_executor_init(&executor, &support->context, 1U, allocator) != RCL_RET_OK) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=executor_init\n");
    goto cleanup_subscription;
  }
  if (rclc_executor_add_subscription(&executor, &subscription, &message, receive_nested,
                                     ON_NEW_DATA) != RCL_RET_OK) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=executor_add_subscription\n");
    goto cleanup_executor;
  }
  for (unsigned int wait = 0U; wait < MAX_WAITS && !received; ++wait) {
    const rcl_ret_t spin = rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
    if (spin != RCL_RET_OK && spin != RCL_RET_TIMEOUT) {
      fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=spin_some rcl_ret=%d\n", spin);
      goto cleanup_executor;
    }
  }
  if (received && received_message.counter.data == ROS_TO_RMW_VALUE &&
      received_message.samples.values[0] == 21U && received_message.samples.values[3] == 24U) {
    printf("RMW_RECEIVED direction=ros_to_rmw reliability=%s value=%u array=%u,%u,%u,%u\n",
           reliability_name, received_message.counter.data, received_message.samples.values[0],
           received_message.samples.values[1], received_message.samples.values[2],
           received_message.samples.values[3]);
    result = 0;
  } else {
    fprintf(stderr, "RMW_CYCLONEDDS_C_TIMEOUT direction=ros_to_rmw value=%u\n",
            received_message.counter.data);
  }

cleanup_executor:
  cleanup_result(rclc_executor_fini(&executor), "executor_fini", &result);
cleanup_subscription:
  cleanup_result(rcl_subscription_fini(&subscription, node), "subscription_fini", &result);
  return result;
}

int main(int argc, char **argv)
{
  if (argc != 3 || (strcmp(argv[1], "pub") != 0 && strcmp(argv[1], "sub") != 0) ||
      (strcmp(argv[2], "best_effort") != 0 && strcmp(argv[2], "reliable") != 0)) {
    fprintf(stderr, "usage: %s pub|sub best_effort|reliable\n", argv[0]);
    return 2;
  }
  rmw_qos_profile_t qos = rmw_qos_profile_sensor_data;
  qos.reliability = strcmp(argv[2], "reliable") == 0 ? RMW_QOS_POLICY_RELIABILITY_RELIABLE
                                                      : RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT;
  int result = 1;
  rcl_allocator_t allocator = rcl_get_default_allocator();
  rclc_support_t support;
  rcl_node_t node = rcl_get_zero_initialized_node();
  if (rclc_support_init(&support, 0, NULL, &allocator) != RCL_RET_OK) {
    return result;
  }
  rcl_node_options_t options = rcl_node_get_default_options();
  options.enable_rosout = false;
  if (rclc_node_init_with_options(&node, "generated_rmw_interop", "", &support, &options) ==
      RCL_RET_OK) {
    result = strcmp(argv[1], "pub") == 0
                 ? run_publisher(&node, &qos, argv[2])
                 : run_subscription(&node, &support, &allocator, &qos, argv[2]);
    cleanup_result(rcl_node_fini(&node), "node_fini", &result);
  }
  cleanup_result(rclc_support_fini(&support), "support_fini", &result);
  return result;
}
