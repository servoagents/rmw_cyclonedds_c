// SPDX-License-Identifier: Apache-2.0

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <std_msgs/msg/u_int32.h>

enum { RMW_TO_ROS_VALUE = 271828182U, ROS_TO_RMW_VALUE = 314159265U, MAX_WAITS = 300 };

static bool received;
static uint32_t received_value;

static void receive_uint32(const void *untyped_message)
{
  const std_msgs__msg__UInt32 *message = untyped_message;
  if (message != NULL) {
    received_value = message->data;
    received = true;
  }
}

static bool pause_100_ms(void)
{
  const struct timespec delay = {.tv_sec = 0, .tv_nsec = 100000000L};
  return nanosleep(&delay, NULL) == 0;
}

static void record_cleanup(const rcl_ret_t cleanup_result, const char *operation, int *result)
{
  if (cleanup_result != RCL_RET_OK) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_CLEANUP_ERROR operation=%s rcl_ret=%d\n", operation,
            cleanup_result);
    *result = 1;
  }
}

static int run_publisher(rcl_node_t *node)
{
  int result = 1;
  rcl_publisher_t publisher = rcl_get_zero_initialized_publisher();
  if (rclc_publisher_init_best_effort(&publisher, node,
                                      ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, UInt32),
                                      "rmw_c_to_ros") != RCL_RET_OK) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=publisher_init\n");
    return result;
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

  const std_msgs__msg__UInt32 message = {.data = RMW_TO_ROS_VALUE};
  for (unsigned int sequence = 1U; sequence <= 5U; ++sequence) {
    if (rcl_publish(&publisher, &message, NULL) != RCL_RET_OK) {
      fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=publish\n");
      goto cleanup;
    }
    printf("RMW_SENT direction=rmw_to_ros value=%u sequence=%u\n", message.data, sequence);
    fflush(stdout);
    if (!pause_100_ms()) {
      fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=publish_delay\n");
      goto cleanup;
    }
  }
  result = 0;

cleanup:
  record_cleanup(rcl_publisher_fini(&publisher, node), "publisher_fini", &result);
  return result;
}

static int run_subscription(rcl_node_t *node, rclc_support_t *support, rcl_allocator_t *allocator)
{
  int result = 1;
  rcl_subscription_t subscription = rcl_get_zero_initialized_subscription();
  rclc_executor_t executor = rclc_executor_get_zero_initialized_executor();
  std_msgs__msg__UInt32 message = {.data = 0U};

  if (rclc_subscription_init_best_effort(&subscription, node,
                                         ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, UInt32),
                                         "ros_to_rmw_c") != RCL_RET_OK) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=subscription_init\n");
    return result;
  }
  if (rclc_executor_init(&executor, &support->context, 1U, allocator) != RCL_RET_OK) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=executor_init\n");
    goto cleanup_subscription;
  }
  if (rclc_executor_add_subscription(&executor, &subscription, &message, receive_uint32,
                                     ON_NEW_DATA) != RCL_RET_OK) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=executor_add_subscription\n");
    goto cleanup_executor;
  }

  for (unsigned int wait = 0U; wait < MAX_WAITS && !received; ++wait) {
    const rcl_ret_t spin_result = rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
    if (spin_result != RCL_RET_OK && spin_result != RCL_RET_TIMEOUT) {
      fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=spin_some rcl_ret=%d\n", spin_result);
      goto cleanup_executor;
    }
  }
  if (received && received_value == ROS_TO_RMW_VALUE) {
    printf("RMW_RECEIVED direction=ros_to_rmw value=%u expected=%u\n", received_value,
           ROS_TO_RMW_VALUE);
    result = 0;
  } else {
    fprintf(stderr, "RMW_CYCLONEDDS_C_TIMEOUT direction=ros_to_rmw value=%u\n", received_value);
  }

cleanup_executor:
  record_cleanup(rclc_executor_fini(&executor), "executor_fini", &result);
cleanup_subscription:
  record_cleanup(rcl_subscription_fini(&subscription, node), "subscription_fini", &result);
  return result;
}

int main(int argc, char **argv)
{
  if (argc != 2 || (strcmp(argv[1], "pub") != 0 && strcmp(argv[1], "sub") != 0)) {
    fprintf(stderr, "usage: %s pub|sub\n", argv[0]);
    return 2;
  }

  int result = 1;
  rcl_allocator_t allocator = rcl_get_default_allocator();
  rclc_support_t support;
  rcl_node_t node = rcl_get_zero_initialized_node();
  if (rclc_support_init(&support, 0, NULL, &allocator) != RCL_RET_OK) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=support_init\n");
    return result;
  }
  rcl_node_options_t node_options = rcl_node_get_default_options();
  node_options.enable_rosout = false;
  if (rclc_node_init_with_options(&node, "rmw_c_interop", "", &support, &node_options) !=
      RCL_RET_OK) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=node_init\n");
    goto cleanup_support;
  }

  result = strcmp(argv[1], "pub") == 0 ? run_publisher(&node)
                                       : run_subscription(&node, &support, &allocator);
  record_cleanup(rcl_node_fini(&node), "node_fini", &result);
cleanup_support:
  record_cleanup(rclc_support_fini(&support), "support_fini", &result);
  return result;
}
