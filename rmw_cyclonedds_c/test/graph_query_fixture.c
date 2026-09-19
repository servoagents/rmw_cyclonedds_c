// SPDX-License-Identifier: Apache-2.0

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <dds/dds.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rcutils/error_handling.h>
#include <rmw/names_and_types.h>
#include <rmw/rmw.h>

#include "UInt32_.h"

enum { MAX_WAIT_TICKS = 400 };

typedef enum expected_state_e {
  EXPECT_INITIAL,
  EXPECT_SECONDARY_GONE,
  EXPECT_PRIMARY_DROP,
  EXPECT_PRIMARY_RESTART,
  EXPECT_ALL_REMOTE_GONE,
} expected_state_t;

typedef struct raw_dds_fixture_s {
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t writer;
  dds_entity_t reader;
} raw_dds_fixture_t;

static bool pause_100_ms(void)
{
  const struct timespec delay = {.tv_sec = 0, .tv_nsec = 100000000L};
  return nanosleep(&delay, NULL) == 0;
}

static bool raw_dds_fixture_init(raw_dds_fixture_t *fixture)
{
  const char *domain_text = getenv("ROS_DOMAIN_ID");
  char *end = NULL;
  const unsigned long domain = domain_text == NULL ? 0UL : strtoul(domain_text, &end, 10);
  if (domain_text != NULL && (end == domain_text || *end != '\0')) {
    return false;
  }
  fixture->participant = dds_create_participant((dds_domainid_t)domain, NULL, NULL);
  if (fixture->participant < 0) {
    return false;
  }
  fixture->topic = dds_create_topic(fixture->participant, &std_msgs_msg_dds__UInt32__desc,
                                    "rt/graph_dds_only", NULL, NULL);
  if (fixture->topic < 0) {
    return false;
  }
  fixture->writer = dds_create_writer(fixture->participant, fixture->topic, NULL, NULL);
  fixture->reader = dds_create_reader(fixture->participant, fixture->topic, NULL, NULL);
  if (fixture->writer < 0 || fixture->reader < 0) {
    return false;
  }
  for (unsigned int tick = 0U; tick < MAX_WAIT_TICKS; ++tick) {
    if (dds_get_matched_subscriptions(fixture->writer, NULL, 0U) > 0) {
      return true;
    }
    if (!pause_100_ms()) {
      break;
    }
  }
  return false;
}

static void raw_dds_fixture_fini(raw_dds_fixture_t *fixture)
{
  if (fixture->participant > 0) {
    (void)dds_delete(fixture->participant);
  }
}

static bool has_node(const rcutils_string_array_t *names, const rcutils_string_array_t *namespaces,
                     const char *name, const char *namespace_)
{
  for (size_t index = 0U; index < names->size; ++index) {
    if (strcmp(names->data[index], name) == 0 &&
        strcmp(namespaces->data[index], namespace_) == 0) {
      return true;
    }
  }
  return false;
}

static bool has_topic_type(const rmw_names_and_types_t *topics, const char *topic,
                           const char *type)
{
  for (size_t topic_index = 0U; topic_index < topics->names.size; ++topic_index) {
    if (strcmp(topics->names.data[topic_index], topic) != 0) {
      continue;
    }
    for (size_t type_index = 0U; type_index < topics->types[topic_index].size; ++type_index) {
      if (strcmp(topics->types[topic_index].data[type_index], type) == 0) {
        return true;
      }
    }
  }
  return false;
}

static bool query_counts(const rmw_node_t *node, const char *topic, size_t publishers,
                         size_t subscribers)
{
  size_t actual_publishers = 0U;
  size_t actual_subscribers = 0U;
  return rmw_count_publishers(node, topic, &actual_publishers) == RMW_RET_OK &&
         rmw_count_subscribers(node, topic, &actual_subscribers) == RMW_RET_OK &&
         actual_publishers == publishers && actual_subscribers == subscribers;
}

static bool state_matches(const rmw_node_t *node, expected_state_t state)
{
  rcutils_string_array_t names = rcutils_get_zero_initialized_string_array();
  rcutils_string_array_t namespaces = rcutils_get_zero_initialized_string_array();
  rmw_names_and_types_t topics = rmw_get_zero_initialized_names_and_types();
  rcutils_allocator_t allocator = rcutils_get_default_allocator();
  bool matches = false;

  if (rmw_get_node_names(node, &names, &namespaces) != RMW_RET_OK ||
      rmw_get_topic_names_and_types(node, &allocator, false, &topics) != RMW_RET_OK) {
    rcutils_reset_error();
    goto cleanup;
  }

  const bool alpha = has_node(&names, &namespaces, "same_name", "/alpha");
  const bool beta = has_node(&names, &namespaces, "same_name", "/beta");
  const bool gamma = has_node(&names, &namespaces, "third", "/gamma");
  const bool secondary = has_node(&names, &namespaces, "separate", "/other");
  const bool local = has_node(&names, &namespaces, "graph_query_fixture", "/");
  const bool topic_a = has_topic_type(&topics, "/graph_remote_a", "std_msgs/msg/UInt32");
  const bool topic_b = has_topic_type(&topics, "/graph_remote_b", "std_msgs/msg/UInt32");
  const bool topic_c = has_topic_type(&topics, "/graph_remote_c", "std_msgs/msg/UInt32");
  const bool dds_only = has_topic_type(&topics, "/graph_dds_only", "std_msgs/msg/UInt32");

  switch (state) {
    case EXPECT_INITIAL:
      matches = local && alpha && beta && gamma && secondary && topic_a && topic_b && topic_c &&
                !dds_only && query_counts(node, "/graph_dds_only", 0U, 0U) &&
                query_counts(node, "/graph_remote_a", 2U, 2U) &&
                query_counts(node, "/graph_remote_b", 1U, 0U) &&
                query_counts(node, "/graph_remote_c", 1U, 0U);
      break;
    case EXPECT_SECONDARY_GONE:
      matches = local && alpha && beta && gamma && !secondary && topic_a && topic_b && !topic_c &&
                query_counts(node, "/graph_remote_c", 0U, 0U);
      break;
    case EXPECT_PRIMARY_DROP:
      matches = local && alpha && !beta && gamma && !secondary && topic_a && topic_b &&
                query_counts(node, "/graph_remote_a", 1U, 0U);
      break;
    case EXPECT_PRIMARY_RESTART:
      matches = local && alpha && beta && gamma && !secondary && topic_a && topic_b &&
                query_counts(node, "/graph_remote_a", 1U, 2U);
      break;
    case EXPECT_ALL_REMOTE_GONE:
      matches = local && !alpha && !beta && !gamma && !secondary && !topic_a && !topic_b &&
                !topic_c && query_counts(node, "/graph_remote_a", 0U, 0U) &&
                query_counts(node, "/graph_remote_b", 0U, 0U) &&
                query_counts(node, "/graph_remote_c", 0U, 0U);
      break;
  }

cleanup:
  if (topics.names.data != NULL) {
    const rmw_ret_t result = rmw_names_and_types_fini(&topics);
    (void)result;
  }
  if (namespaces.data != NULL) {
    const rcutils_ret_t result = rcutils_string_array_fini(&namespaces);
    (void)result;
  }
  if (names.data != NULL) {
    const rcutils_ret_t result = rcutils_string_array_fini(&names);
    (void)result;
  }
  return matches;
}

static bool wait_for_state(const rmw_node_t *node, expected_state_t state, const char *marker)
{
  for (unsigned int tick = 0U; tick < MAX_WAIT_TICKS; ++tick) {
    if (state_matches(node, state)) {
      puts(marker);
      fflush(stdout);
      return true;
    }
    if (!pause_100_ms()) {
      break;
    }
  }
  rcutils_string_array_t names = rcutils_get_zero_initialized_string_array();
  rcutils_string_array_t namespaces = rcutils_get_zero_initialized_string_array();
  if (rmw_get_node_names(node, &names, &namespaces) == RMW_RET_OK) {
    for (size_t index = 0U; index < names.size; ++index) {
      fprintf(stderr, "GRAPH_QUERY_NODE namespace=%s name=%s\n", namespaces.data[index],
              names.data[index]);
    }
  }
  size_t publishers = 0U;
  size_t subscribers = 0U;
  if (rmw_count_publishers(node, "/graph_remote_a", &publishers) == RMW_RET_OK &&
      rmw_count_subscribers(node, "/graph_remote_a", &subscribers) == RMW_RET_OK) {
    fprintf(stderr, "GRAPH_QUERY_COUNTS topic=/graph_remote_a publishers=%zu subscribers=%zu\n",
            publishers, subscribers);
  }
  if (namespaces.data != NULL) {
    const rcutils_ret_t cleanup = rcutils_string_array_fini(&namespaces);
    (void)cleanup;
  }
  if (names.data != NULL) {
    const rcutils_ret_t cleanup = rcutils_string_array_fini(&names);
    (void)cleanup;
  }
  fprintf(stderr, "GRAPH_QUERY_TIMEOUT state=%d\n", (int)state);
  return false;
}

int main(void)
{
  int result = 1;
  rcl_allocator_t allocator = rcl_get_default_allocator();
  rclc_support_t support;
  rcl_node_t node = rcl_get_zero_initialized_node();
  raw_dds_fixture_t raw_dds = {0};
  if (rclc_support_init(&support, 0, NULL, &allocator) != RCL_RET_OK) {
    return result;
  }
  rcl_node_options_t options = rcl_node_get_default_options();
  options.enable_rosout = false;
  if (rclc_node_init_with_options(&node, "graph_query_fixture", "/", &support, &options) !=
      RCL_RET_OK) {
    goto fini_support;
  }
  if (!raw_dds_fixture_init(&raw_dds)) {
    goto fini_node;
  }

  const rmw_node_t *rmw_node = rcl_node_get_rmw_handle(&node);
  if (rmw_node != NULL &&
      wait_for_state(rmw_node, EXPECT_INITIAL, "GRAPH_QUERY_PASS state=initial") &&
      wait_for_state(rmw_node, EXPECT_SECONDARY_GONE,
                     "GRAPH_QUERY_PASS state=secondary_gone") &&
      wait_for_state(rmw_node, EXPECT_PRIMARY_DROP, "GRAPH_QUERY_PASS state=primary_drop") &&
      wait_for_state(rmw_node, EXPECT_PRIMARY_RESTART,
                     "GRAPH_QUERY_PASS state=primary_restart") &&
      wait_for_state(rmw_node, EXPECT_ALL_REMOTE_GONE,
                     "GRAPH_QUERY_PASS state=all_remote_gone")) {
    result = 0;
  }

fini_node:
  raw_dds_fixture_fini(&raw_dds);
  if (rcl_node_fini(&node) != RCL_RET_OK) {
    result = 1;
  }
fini_support:
  if (rclc_support_fini(&support) != RCL_RET_OK) {
    result = 1;
  }
  return result;
}
