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

enum { SAMPLE_COUNT = 5, MATCH_ATTEMPTS = 300, RECEIVE_ATTEMPTS = 300 };

static bool received[SAMPLE_COUNT];

static bool pause_ms(long milliseconds)
{
  const struct timespec delay = {
      .tv_sec = milliseconds / 1000L,
      .tv_nsec = (milliseconds % 1000L) * 1000000L,
  };
  return nanosleep(&delay, NULL) == 0;
}

static void receive_sample(const void *untyped_message)
{
  const cyclonedds_c_test_msgs__msg__NestedFixed *message = untyped_message;
  if (message != NULL && message->counter.data >= 1U && message->counter.data <= SAMPLE_COUNT) {
    received[message->counter.data - 1U] = true;
    printf("RMW_RELIABLE_LOSS_RECEIVED sequence=%u\n", message->counter.data);
    fflush(stdout);
  }
}

static void record_cleanup(rcl_ret_t cleanup, const char *operation, int *result)
{
  if (cleanup != RCL_RET_OK) {
    fprintf(stderr, "RMW_RELIABLE_LOSS_CLEANUP_ERROR operation=%s rcl_ret=%d\n", operation,
            cleanup);
    *result = 1;
  }
}

static rmw_qos_profile_t reliable_qos(void)
{
  rmw_qos_profile_t qos = rmw_qos_profile_sensor_data;
  qos.depth = SAMPLE_COUNT;
  qos.reliability = RMW_QOS_POLICY_RELIABILITY_RELIABLE;
  return qos;
}

static int run_publisher(rcl_node_t *node)
{
  int result = 1;
  rcl_publisher_t publisher = rcl_get_zero_initialized_publisher();
  const rmw_qos_profile_t qos = reliable_qos();
  if (rclc_publisher_init(
          &publisher, node, ROSIDL_GET_MSG_TYPE_SUPPORT(cyclonedds_c_test_msgs, msg, NestedFixed),
          "reliable_loss", &qos) != RCL_RET_OK) {
    fprintf(stderr, "RMW_RELIABLE_LOSS_ERROR operation=publisher_init\n");
    return result;
  }

  size_t matched = 0U;
  for (unsigned int attempt = 0U; attempt < MATCH_ATTEMPTS && matched == 0U; ++attempt) {
    if (rcl_publisher_get_subscription_count(&publisher, &matched) != RCL_RET_OK ||
        !pause_ms(100L)) {
      fprintf(stderr, "RMW_RELIABLE_LOSS_ERROR operation=wait_for_subscription\n");
      goto cleanup;
    }
  }
  if (matched == 0U) {
    fprintf(stderr, "RMW_RELIABLE_LOSS_ERROR operation=match_timeout\n");
    goto cleanup;
  }

  printf("RMW_RELIABLE_LOSS_MATCH role=pub\n");
  fflush(stdout);
  if (!pause_ms(3000L)) {
    goto cleanup;
  }

  cyclonedds_c_test_msgs__msg__NestedFixed message = {0};
  for (uint32_t sequence = 1U; sequence <= SAMPLE_COUNT; ++sequence) {
    message.counter.data = sequence;
    if (rcl_publish(&publisher, &message, NULL) != RCL_RET_OK) {
      fprintf(stderr, "RMW_RELIABLE_LOSS_ERROR operation=publish sequence=%u\n", sequence);
      goto cleanup;
    }
    printf("RMW_RELIABLE_LOSS_SENT sequence=%u\n", sequence);
    fflush(stdout);
    if (!pause_ms(sequence == 1U ? 1000L : 100L)) {
      goto cleanup;
    }
  }
  if (!pause_ms(2000L)) {
    goto cleanup;
  }
  printf("RMW_RELIABLE_LOSS_PASS role=pub samples=%u\n", SAMPLE_COUNT);
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
  cyclonedds_c_test_msgs__msg__NestedFixed message = {0};
  const rmw_qos_profile_t qos = reliable_qos();
  if (rclc_subscription_init(
          &subscription, node,
          ROSIDL_GET_MSG_TYPE_SUPPORT(cyclonedds_c_test_msgs, msg, NestedFixed), "reliable_loss",
          &qos) != RCL_RET_OK) {
    fprintf(stderr, "RMW_RELIABLE_LOSS_ERROR operation=subscription_init\n");
    return result;
  }
  if (rclc_executor_init(&executor, &support->context, 1U, allocator) != RCL_RET_OK) {
    fprintf(stderr, "RMW_RELIABLE_LOSS_ERROR operation=executor_init\n");
    goto cleanup_subscription;
  }
  if (rclc_executor_add_subscription(&executor, &subscription, &message, receive_sample,
                                     ON_NEW_DATA) != RCL_RET_OK) {
    fprintf(stderr, "RMW_RELIABLE_LOSS_ERROR operation=executor_add_subscription\n");
    goto cleanup_executor;
  }

  for (unsigned int attempt = 0U; attempt < RECEIVE_ATTEMPTS; ++attempt) {
    bool complete = true;
    for (size_t index = 0U; index < SAMPLE_COUNT; ++index) {
      complete = complete && received[index];
    }
    if (complete) {
      printf("RMW_RELIABLE_LOSS_PASS role=sub samples=%u first_sample_recovered=1\n", SAMPLE_COUNT);
      result = 0;
      break;
    }
    const rcl_ret_t spin = rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
    if (spin != RCL_RET_OK && spin != RCL_RET_TIMEOUT) {
      fprintf(stderr, "RMW_RELIABLE_LOSS_ERROR operation=spin_some rcl_ret=%d\n", spin);
      break;
    }
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
    return result;
  }
  rcl_node_options_t options = rcl_node_get_default_options();
  options.enable_rosout = false;
  if (rclc_node_init_with_options(&node, "reliable_loss_test", "", &support, &options) ==
      RCL_RET_OK) {
    result = strcmp(argv[1], "pub") == 0 ? run_publisher(&node)
                                         : run_subscription(&node, &support, &allocator);
    record_cleanup(rcl_node_fini(&node), "node_fini", &result);
  }
  record_cleanup(rclc_support_fini(&support), "support_fini", &result);
  return result;
}
