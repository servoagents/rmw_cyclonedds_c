// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>

#include <rcl/error_handling.h>
#include <rcl/guard_condition.h>
#include <rcl/rcl.h>
#include <rcl/wait.h>
#include <rclc/rclc.h>
#include <rmw/qos_profiles.h>
#include <std_msgs/msg/int32.h>
#include <std_msgs/msg/u_int32.h>

static int fail(const char *operation, rcl_ret_t result)
{
  fprintf(stderr, "RMW_CYCLONEDDS_C_CONTRACT_FAIL operation=%s rcl_ret=%d\n", operation, result);
  return 1;
}

static int expect_creation_failure(rcl_node_t *node,
                                   const rosidl_message_type_support_t *type_support,
                                   const char *topic, rmw_qos_profile_t qos)
{
  rcl_publisher_t publisher = rcl_get_zero_initialized_publisher();
  rcl_publisher_options_t options = rcl_publisher_get_default_options();
  options.qos = qos;
  const rcl_ret_t result = rcl_publisher_init(&publisher, node, type_support, topic, &options);
  if (result == RCL_RET_OK) {
    const rcl_ret_t cleanup_result = rcl_publisher_fini(&publisher, node);
    if (cleanup_result != RCL_RET_OK) {
      return fail("unsupported_publisher_fini", cleanup_result);
    }
    return fail("unsupported_publisher_was_created", result);
  }
  rcl_reset_error();
  return 0;
}

int main(void)
{
  int result = 1;
  rcl_allocator_t allocator = rcl_get_default_allocator();
  rclc_support_t support;
  rcl_node_t node = rcl_get_zero_initialized_node();
  rcl_guard_condition_t guard_condition = rcl_get_zero_initialized_guard_condition();
  rcl_wait_set_t wait_set = rcl_get_zero_initialized_wait_set();

  if (rclc_support_init(&support, 0, NULL, &allocator) != RCL_RET_OK) {
    return fail("support_init", RCL_RET_ERROR);
  }
  rcl_node_options_t node_options = rcl_node_get_default_options();
  node_options.enable_rosout = false;
  rcl_ret_t call_result =
      rclc_node_init_with_options(&node, "contract_test", "", &support, &node_options);
  if (call_result != RCL_RET_OK) {
    result = fail("node_init", call_result);
    goto cleanup_support;
  }

  rmw_qos_profile_t reliable_qos = rmw_qos_profile_sensor_data;
  reliable_qos.reliability = RMW_QOS_POLICY_RELIABILITY_RELIABLE;
  if (expect_creation_failure(&node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, UInt32),
                              "reliable_rejected", reliable_qos) != 0) {
    goto cleanup_node;
  }
  if (expect_creation_failure(&node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
                              "type_rejected", rmw_qos_profile_sensor_data) != 0) {
    goto cleanup_node;
  }

  call_result = rcl_guard_condition_init(&guard_condition, &support.context,
                                         rcl_guard_condition_get_default_options());
  if (call_result != RCL_RET_OK) {
    result = fail("guard_condition_init", call_result);
    goto cleanup_node;
  }
  call_result = rcl_wait_set_init(&wait_set, 0U, 1U, 0U, 0U, 0U, 0U, &support.context, allocator);
  if (call_result != RCL_RET_OK) {
    result = fail("wait_set_init", call_result);
    goto cleanup_guard;
  }
  call_result = rcl_wait_set_add_guard_condition(&wait_set, &guard_condition, NULL);
  if (call_result != RCL_RET_OK) {
    result = fail("add_guard_for_timeout", call_result);
    goto cleanup_wait_set;
  }
  call_result = rcl_wait(&wait_set, RCL_MS_TO_NS(10));
  if (call_result != RCL_RET_TIMEOUT || wait_set.guard_conditions[0] != NULL) {
    result = fail("wait_timeout", call_result);
    goto cleanup_wait_set;
  }

  call_result = rcl_wait_set_clear(&wait_set);
  if (call_result == RCL_RET_OK) {
    call_result = rcl_wait_set_add_guard_condition(&wait_set, &guard_condition, NULL);
  }
  if (call_result == RCL_RET_OK) {
    call_result = rcl_trigger_guard_condition(&guard_condition);
  }
  if (call_result == RCL_RET_OK) {
    call_result = rcl_wait(&wait_set, RCL_MS_TO_NS(1000));
  }
  if (call_result != RCL_RET_OK || wait_set.guard_conditions[0] == NULL) {
    result = fail("guard_wakeup", call_result);
    goto cleanup_wait_set;
  }

  printf("RMW_CYCLONEDDS_C_CONTRACT_PASS reliable=rejected type=rejected timeout=passed "
         "guard=passed\n");
  result = 0;

cleanup_wait_set:
  if (rcl_wait_set_fini(&wait_set) != RCL_RET_OK) {
    result = fail("wait_set_fini", RCL_RET_ERROR);
  }
cleanup_guard:
  if (rcl_guard_condition_fini(&guard_condition) != RCL_RET_OK) {
    result = fail("guard_condition_fini", RCL_RET_ERROR);
  }
cleanup_node:
  if (rcl_node_fini(&node) != RCL_RET_OK) {
    result = fail("node_fini", RCL_RET_ERROR);
  }
cleanup_support:
  if (rclc_support_fini(&support) != RCL_RET_OK) {
    result = fail("support_fini", RCL_RET_ERROR);
  }
  return result;
}
