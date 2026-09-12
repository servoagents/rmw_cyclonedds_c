// SPDX-License-Identifier: Apache-2.0

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <std_msgs/msg/u_int32.h>

enum { TEST_VALUE = 42424242U, TEST_MAX_SPINS = 100 };

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

static int check_rcl(const rcl_ret_t result, const char *operation)
{
  if (result == RCL_RET_OK) {
    return 0;
  }
  fprintf(stderr, "RMW_CYCLONEDDS_C_ERROR operation=%s rcl_ret=%d\n", operation, result);
  return 1;
}

static void record_cleanup(const rcl_ret_t cleanup_result, const char *operation, int *result)
{
  if (cleanup_result != RCL_RET_OK) {
    fprintf(stderr, "RMW_CYCLONEDDS_C_CLEANUP_ERROR operation=%s rcl_ret=%d\n", operation,
            cleanup_result);
    *result = 1;
  }
}

int main(void)
{
  int result = 1;
  rcl_allocator_t allocator = rcl_get_default_allocator();
  rclc_support_t support;
  rcl_node_t node = rcl_get_zero_initialized_node();
  rcl_publisher_t publisher = rcl_get_zero_initialized_publisher();
  rcl_subscription_t subscription = rcl_get_zero_initialized_subscription();
  rclc_executor_t executor = rclc_executor_get_zero_initialized_executor();
  std_msgs__msg__UInt32 outbound = {.data = TEST_VALUE};
  std_msgs__msg__UInt32 inbound = {.data = 0U};

  if (check_rcl(rclc_support_init(&support, 0, NULL, &allocator), "support_init") != 0) {
    return result;
  }

  rcl_node_options_t node_options = rcl_node_get_default_options();
  node_options.enable_rosout = false;
  if (check_rcl(rclc_node_init_with_options(&node, "rmw_c_smoke", "", &support, &node_options),
                "node_init") != 0) {
    goto fini_support;
  }

  if (check_rcl(rclc_publisher_init_best_effort(&publisher, &node,
                                                ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, UInt32),
                                                "rmw_c_loopback"),
                "publisher_init") != 0) {
    goto fini_node;
  }

  if (check_rcl(rclc_subscription_init_best_effort(
                    &subscription, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, UInt32),
                    "rmw_c_loopback"),
                "subscription_init") != 0) {
    goto fini_publisher;
  }

  if (check_rcl(rclc_executor_init(&executor, &support.context, 1U, &allocator), "executor_init") !=
      0) {
    goto fini_subscription;
  }
  if (check_rcl(rclc_executor_add_subscription(&executor, &subscription, &inbound, receive_uint32,
                                               ON_NEW_DATA),
                "executor_add_subscription") != 0) {
    goto fini_executor;
  }

  for (unsigned int spin = 0U; spin < TEST_MAX_SPINS && !received; ++spin) {
    if (check_rcl(rcl_publish(&publisher, &outbound, NULL), "publish") != 0) {
      goto fini_executor;
    }
    if (check_rcl(rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100)), "spin_some") != 0) {
      goto fini_executor;
    }
  }

  if (received && received_value == TEST_VALUE) {
    printf("RMW_CYCLONEDDS_C_SMOKE_PASS value=%u\n", received_value);
    result = 0;
  } else {
    fprintf(stderr, "RMW_CYCLONEDDS_C_TIMEOUT received=%d value=%u\n", received, received_value);
  }

fini_executor:
  record_cleanup(rclc_executor_fini(&executor), "executor_fini", &result);
fini_subscription:
  record_cleanup(rcl_subscription_fini(&subscription, &node), "subscription_fini", &result);
fini_publisher:
  record_cleanup(rcl_publisher_fini(&publisher, &node), "publisher_fini", &result);
fini_node:
  record_cleanup(rcl_node_fini(&node), "node_fini", &result);
fini_support:
  record_cleanup(rclc_support_fini(&support), "support_fini", &result);
  return result;
}
