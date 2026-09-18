// SPDX-License-Identifier: Apache-2.0

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <cyclonedds_c_test_msgs/msg/nested_fixed.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rmw/qos_profiles.h>

enum { HISTORY_LAST_VALUE = 5U, LIVE_VALUE = 6U, MAX_WAITS = 300U };

static bool pause_100_ms(void)
{
  const struct timespec delay = {.tv_sec = 0, .tv_nsec = 100000000L};
  return nanosleep(&delay, NULL) == 0;
}

static bool allow_history_delivery(void)
{
  for (unsigned int wait = 0U; wait < 5U; ++wait) {
    if (!pause_100_ms()) {
      return false;
    }
  }
  return true;
}

static void record_cleanup(rcl_ret_t cleanup, const char *operation, int *result)
{
  if (cleanup != RCL_RET_OK) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_CLEANUP_ERROR operation=%s rcl_ret=%d\n", operation,
            cleanup);
    *result = 1;
  }
}

static rmw_qos_profile_t transient_local_qos(size_t depth)
{
  rmw_qos_profile_t qos = rmw_qos_profile_default;
  qos.history = RMW_QOS_POLICY_HISTORY_KEEP_LAST;
  qos.depth = depth;
  qos.reliability = RMW_QOS_POLICY_RELIABILITY_RELIABLE;
  qos.durability = RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL;
  return qos;
}

static bool actual_qos_matches(const rmw_qos_profile_t *actual, size_t depth)
{
  return actual != NULL && actual->history == RMW_QOS_POLICY_HISTORY_KEEP_LAST &&
         actual->depth == depth &&
         actual->reliability == RMW_QOS_POLICY_RELIABILITY_RELIABLE &&
         actual->durability == RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL;
}

static rcl_ret_t publish_value(rcl_publisher_t *publisher, uint32_t value)
{
  cyclonedds_c_test_msgs__msg__NestedFixed message = {0};
  message.counter.data = value;
  for (size_t index = 0U; index < 4U; ++index) {
    message.samples.values[index] = value * UINT32_C(10) + (uint32_t)index;
  }
  return rcl_publish(publisher, &message, NULL);
}

static bool wait_for_match(rcl_publisher_t *publisher, rcl_subscription_t *subscription)
{
  size_t subscriptions = 0U;
  size_t publishers = 0U;
  for (unsigned int wait = 0U; wait < MAX_WAITS; ++wait) {
    if (rcl_publisher_get_subscription_count(publisher, &subscriptions) != RCL_RET_OK ||
        rcl_subscription_get_publisher_count(subscription, &publishers) != RCL_RET_OK) {
      return false;
    }
    if (subscriptions > 0U && publishers > 0U) {
      return true;
    }
    if (!pause_100_ms()) {
      return false;
    }
  }
  return false;
}

static bool wait_for_publisher(rcl_subscription_t *subscription)
{
  size_t publishers = 0U;
  for (unsigned int wait = 0U; wait < MAX_WAITS; ++wait) {
    if (rcl_subscription_get_publisher_count(subscription, &publishers) != RCL_RET_OK) {
      return false;
    }
    if (publishers > 0U) {
      return true;
    }
    if (!pause_100_ms()) {
      return false;
    }
  }
  return false;
}

static bool wait_for_subscription(rcl_publisher_t *publisher)
{
  size_t subscriptions = 0U;
  for (unsigned int wait = 0U; wait < MAX_WAITS; ++wait) {
    if (rcl_publisher_get_subscription_count(publisher, &subscriptions) != RCL_RET_OK) {
      return false;
    }
    if (subscriptions > 0U) {
      return true;
    }
    if (!pause_100_ms()) {
      return false;
    }
  }
  return false;
}

static bool wait_for_subscription_cleanup(rcl_publisher_t *publisher)
{
  size_t subscriptions = 0U;
  for (unsigned int wait = 0U; wait < MAX_WAITS; ++wait) {
    if (rcl_publisher_get_subscription_count(publisher, &subscriptions) != RCL_RET_OK) {
      return false;
    }
    if (subscriptions == 0U) {
      return true;
    }
    if (!pause_100_ms()) {
      return false;
    }
  }
  return false;
}

static bool take_value(rcl_subscription_t *subscription, uint32_t *value)
{
  cyclonedds_c_test_msgs__msg__NestedFixed message = {0};
  for (unsigned int wait = 0U; wait < MAX_WAITS; ++wait) {
    rmw_message_info_t message_info = rmw_get_zero_initialized_message_info();
    const rcl_ret_t result = rcl_take(subscription, &message, &message_info, NULL);
    if (result == RCL_RET_OK) {
      *value = message.counter.data;
      return true;
    }
    if (result != RCL_RET_SUBSCRIPTION_TAKE_FAILED || !pause_100_ms()) {
      return false;
    }
  }
  return false;
}

static int verify_received_sequence(rcl_subscription_t *subscription, size_t depth,
                                    bool include_live)
{
  const uint32_t first = (uint32_t)(HISTORY_LAST_VALUE + 1U - depth);
  const size_t count = depth + (include_live ? 1U : 0U);
  for (size_t index = 0U; index < count; ++index) {
    const uint32_t expected = first + (uint32_t)index;
    uint32_t received = 0U;
    if (!take_value(subscription, &received) || received != expected) {
      fprintf(stderr,
              "RMW_TRANSIENT_LOCAL_SEQUENCE_ERROR depth=%zu index=%zu expected=%u received=%u\n",
              depth, index, expected, received);
      return 1;
    }
  }
  return 0;
}

static int run_self_case(rcl_node_t *node, size_t depth)
{
  char topic[96];
  (void)snprintf(topic, sizeof(topic), "transient_local_self_depth_%zu", depth);
  const rmw_qos_profile_t qos = transient_local_qos(depth);
  rcl_publisher_t publisher = rcl_get_zero_initialized_publisher();
  rcl_subscription_t subscription = rcl_get_zero_initialized_subscription();
  int result = 1;

  if (rclc_publisher_init(
          &publisher, node, ROSIDL_GET_MSG_TYPE_SUPPORT(cyclonedds_c_test_msgs, msg, NestedFixed),
          topic, &qos) != RCL_RET_OK) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=publisher_init depth=%zu\n", depth);
    return result;
  }
  if (!actual_qos_matches(rcl_publisher_get_actual_qos(&publisher), depth)) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=publisher_actual_qos depth=%zu\n",
            depth);
    goto cleanup_publisher;
  }
  for (uint32_t value = 1U; value <= HISTORY_LAST_VALUE; ++value) {
    if (publish_value(&publisher, value) != RCL_RET_OK) {
      fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=publish_history depth=%zu\n", depth);
      goto cleanup_publisher;
    }
  }

  if (rclc_subscription_init(
          &subscription, node,
          ROSIDL_GET_MSG_TYPE_SUPPORT(cyclonedds_c_test_msgs, msg, NestedFixed), topic,
          &qos) != RCL_RET_OK) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=subscription_init depth=%zu\n", depth);
    goto cleanup_publisher;
  }
  if (!actual_qos_matches(rcl_subscription_get_actual_qos(&subscription), depth)) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=subscription_actual_qos depth=%zu\n",
            depth);
    goto cleanup_subscription;
  }
  if (!wait_for_match(&publisher, &subscription)) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_TIMEOUT operation=match depth=%zu\n", depth);
    goto cleanup_subscription;
  }
  if (verify_received_sequence(&subscription, depth, false) != 0) {
    goto cleanup_subscription;
  }
  if (publish_value(&publisher, LIVE_VALUE) != RCL_RET_OK) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=publish_live depth=%zu\n", depth);
    goto cleanup_subscription;
  }
  uint32_t live = 0U;
  if (!take_value(&subscription, &live) || live != LIVE_VALUE) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_SEQUENCE_ERROR depth=%zu expected=%u received=%u\n",
            depth, LIVE_VALUE, live);
    goto cleanup_subscription;
  }

  printf("RMW_TRANSIENT_LOCAL_PASS direction=c_to_c depth=%zu history_first=%zu "
         "history_last=%u live=%u\n",
         depth, HISTORY_LAST_VALUE + 1U - depth, HISTORY_LAST_VALUE, LIVE_VALUE);
  result = 0;

cleanup_subscription:
  record_cleanup(rcl_subscription_fini(&subscription, node), "subscription_fini", &result);
cleanup_publisher:
  record_cleanup(rcl_publisher_fini(&publisher, node), "publisher_fini", &result);
  return result;
}

static int run_publisher(rcl_node_t *node, size_t depth)
{
  char topic[96];
  (void)snprintf(topic, sizeof(topic), "transient_local_rmw_to_ros_depth_%zu", depth);
  const rmw_qos_profile_t qos = transient_local_qos(depth);
  rcl_publisher_t publisher = rcl_get_zero_initialized_publisher();
  int result = 1;

  if (rclc_publisher_init(
          &publisher, node, ROSIDL_GET_MSG_TYPE_SUPPORT(cyclonedds_c_test_msgs, msg, NestedFixed),
          topic, &qos) != RCL_RET_OK) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=publisher_init depth=%zu\n", depth);
    return result;
  }
  if (!actual_qos_matches(rcl_publisher_get_actual_qos(&publisher), depth)) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=publisher_actual_qos depth=%zu\n",
            depth);
    goto cleanup;
  }
  for (uint32_t value = 1U; value <= HISTORY_LAST_VALUE; ++value) {
    if (publish_value(&publisher, value) != RCL_RET_OK) {
      fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=publish_history depth=%zu\n", depth);
      goto cleanup;
    }
  }
  printf("RMW_TRANSIENT_LOCAL_READY role=pub depth=%zu history_last=%u\n", depth,
         HISTORY_LAST_VALUE);
  fflush(stdout);
  if (!wait_for_subscription(&publisher)) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_TIMEOUT operation=wait_for_subscription depth=%zu\n",
            depth);
    goto cleanup;
  }
  if (!allow_history_delivery()) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=history_delivery depth=%zu\n", depth);
    goto cleanup;
  }
  if (publish_value(&publisher, LIVE_VALUE) != RCL_RET_OK) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=publish_live depth=%zu\n", depth);
    goto cleanup;
  }
  if (!wait_for_subscription_cleanup(&publisher)) {
    fprintf(stderr,
            "RMW_TRANSIENT_LOCAL_TIMEOUT operation=wait_for_subscription_cleanup depth=%zu\n",
            depth);
    goto cleanup;
  }
  printf("RMW_TRANSIENT_LOCAL_PASS direction=c_to_stock depth=%zu live=%u\n", depth,
         LIVE_VALUE);
  result = 0;

cleanup:
  record_cleanup(rcl_publisher_fini(&publisher, node), "publisher_fini", &result);
  return result;
}

static int run_subscription(rcl_node_t *node, size_t depth)
{
  char topic[96];
  (void)snprintf(topic, sizeof(topic), "transient_local_ros_to_rmw_depth_%zu", depth);
  const rmw_qos_profile_t qos = transient_local_qos(depth);
  rcl_subscription_t subscription = rcl_get_zero_initialized_subscription();
  int result = 1;

  if (rclc_subscription_init(
          &subscription, node,
          ROSIDL_GET_MSG_TYPE_SUPPORT(cyclonedds_c_test_msgs, msg, NestedFixed), topic,
          &qos) != RCL_RET_OK) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=subscription_init depth=%zu\n", depth);
    return result;
  }
  if (!actual_qos_matches(rcl_subscription_get_actual_qos(&subscription), depth)) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=subscription_actual_qos depth=%zu\n",
            depth);
    goto cleanup;
  }
  if (!wait_for_publisher(&subscription)) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_TIMEOUT operation=wait_for_publisher depth=%zu\n",
            depth);
    goto cleanup;
  }
  if (verify_received_sequence(&subscription, depth, true) != 0) {
    goto cleanup;
  }
  printf("RMW_TRANSIENT_LOCAL_PASS direction=stock_to_c depth=%zu history_first=%zu "
         "history_last=%u live=%u\n",
         depth, HISTORY_LAST_VALUE + 1U - depth, HISTORY_LAST_VALUE, LIVE_VALUE);
  result = 0;

cleanup:
  record_cleanup(rcl_subscription_fini(&subscription, node), "subscription_fini", &result);
  return result;
}

int main(int argc, char **argv)
{
  const bool self_test = argc == 1;
  if (!self_test &&
      (argc != 3 || (strcmp(argv[1], "pub") != 0 && strcmp(argv[1], "sub") != 0) ||
       (strcmp(argv[2], "1") != 0 && strcmp(argv[2], "3") != 0))) {
    fprintf(stderr, "usage: %s [pub|sub 1|3]\n", argv[0]);
    return 2;
  }
  const size_t depth = self_test ? 0U : (size_t)strtoul(argv[2], NULL, 10);
  rcl_allocator_t allocator = rcl_get_default_allocator();
  rclc_support_t support;
  rcl_node_t node = rcl_get_zero_initialized_node();
  int result = 1;

  if (rclc_support_init(&support, 0, NULL, &allocator) != RCL_RET_OK) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=support_init\n");
    return result;
  }
  rcl_node_options_t options = rcl_node_get_default_options();
  options.enable_rosout = false;
  if (rclc_node_init_with_options(&node, "transient_local_test", "", &support, &options) !=
      RCL_RET_OK) {
    fprintf(stderr, "RMW_TRANSIENT_LOCAL_ERROR operation=node_init\n");
    goto cleanup_support;
  }

  if (self_test) {
    result = run_self_case(&node, 1U);
    if (result == 0) {
      result = run_self_case(&node, 3U);
    }
  } else {
    result = strcmp(argv[1], "pub") == 0 ? run_publisher(&node, depth)
                                          : run_subscription(&node, depth);
  }

  record_cleanup(rcl_node_fini(&node), "node_fini", &result);
cleanup_support:
  record_cleanup(rclc_support_fini(&support), "support_fini", &result);
  return result;
}
