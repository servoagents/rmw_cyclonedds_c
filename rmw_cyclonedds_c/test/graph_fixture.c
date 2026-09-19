// SPDX-License-Identifier: Apache-2.0

#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include <rcutils/error_handling.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <std_msgs/msg/u_int32.h>

enum { MAX_WAIT_TICKS = 1200, CONTROL_PATH_CAPACITY = 512 };

typedef struct fixture_s {
  rcl_node_t node;
  rcl_publisher_t publishers[2];
  rcl_subscription_t subscriptions[2];
  bool publisher_active[2];
  bool subscription_active[2];
} fixture_t;

static bool pause_100_ms(void)
{
  const struct timespec delay = {.tv_sec = 0, .tv_nsec = 100000000L};
  return nanosleep(&delay, NULL) == 0;
}

static bool command_exists(const char *control_directory, const char *command)
{
  char path[CONTROL_PATH_CAPACITY];
  const int length = snprintf(path, sizeof(path), "%s/%s", control_directory, command);
  if (length < 0 || (size_t)length >= sizeof(path)) {
    return false;
  }
  struct stat status;
  return stat(path, &status) == 0;
}

static int check_rcl(rcl_ret_t result, const char *operation)
{
  if (result == RCL_RET_OK) {
    return 0;
  }
  fprintf(stderr, "GRAPH_FIXTURE_ERROR operation=%s rcl_ret=%d detail=%s\n", operation, result,
          rcutils_get_error_string().str);
  rcutils_reset_error();
  return 1;
}

static int create_publisher(fixture_t *fixture, size_t index, const char *topic)
{
  if (check_rcl(rclc_publisher_init_default(
                    &fixture->publishers[index], &fixture->node,
                    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, UInt32), topic),
                "publisher_init") != 0) {
    return 1;
  }
  fixture->publisher_active[index] = true;
  return 0;
}

static int create_subscription(fixture_t *fixture, size_t index, const char *topic)
{
  if (check_rcl(rclc_subscription_init_default(
                    &fixture->subscriptions[index], &fixture->node,
                    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, UInt32), topic),
                "subscription_init") != 0) {
    return 1;
  }
  fixture->subscription_active[index] = true;
  return 0;
}

static int destroy_publisher(fixture_t *fixture, size_t index)
{
  if (!fixture->publisher_active[index]) {
    return 0;
  }
  if (check_rcl(rcl_publisher_fini(&fixture->publishers[index], &fixture->node),
                "publisher_fini") != 0) {
    return 1;
  }
  fixture->publisher_active[index] = false;
  return 0;
}

static int destroy_subscription(fixture_t *fixture, size_t index)
{
  if (!fixture->subscription_active[index]) {
    return 0;
  }
  if (check_rcl(rcl_subscription_fini(&fixture->subscriptions[index], &fixture->node),
                "subscription_fini") != 0) {
    return 1;
  }
  fixture->subscription_active[index] = false;
  return 0;
}

static int create_topology(fixture_t *fixture, const char *topology)
{
  if (strcmp(topology, "node") == 0 || strcmp(topology, "lifecycle") == 0) {
    return 0;
  }
  if (strcmp(topology, "publisher") == 0) {
    return create_publisher(fixture, 0U, "graph_alpha");
  }
  if (strcmp(topology, "subscriber") == 0) {
    return create_subscription(fixture, 0U, "graph_alpha");
  }
  if (strcmp(topology, "both") == 0) {
    return create_publisher(fixture, 0U, "graph_alpha") ||
           create_subscription(fixture, 0U, "graph_alpha");
  }
  if (strcmp(topology, "two_publishers") == 0) {
    return create_publisher(fixture, 0U, "graph_alpha") ||
           create_publisher(fixture, 1U, "graph_alpha");
  }
  if (strcmp(topology, "two_subscriptions") == 0) {
    return create_subscription(fixture, 0U, "graph_alpha") ||
           create_subscription(fixture, 1U, "graph_alpha");
  }
  if (strcmp(topology, "multiple_topics") == 0) {
    return create_publisher(fixture, 0U, "graph_alpha") ||
           create_subscription(fixture, 0U, "graph_beta");
  }
  fprintf(stderr, "unknown topology: %s\n", topology);
  return 1;
}

static int run_lifecycle(fixture_t *fixture, const char *control_directory)
{
  bool publisher_created = false;
  bool subscription_created = false;
  bool publisher_destroyed = false;
  bool subscription_destroyed = false;

  for (unsigned int tick = 0U; tick < MAX_WAIT_TICKS; ++tick) {
    if (!publisher_created && command_exists(control_directory, "create_publisher")) {
      if (create_publisher(fixture, 0U, "graph_alpha") != 0) {
        return 1;
      }
      publisher_created = true;
      puts("GRAPH_FIXTURE_STATE publisher");
      fflush(stdout);
    }
    if (publisher_created && !subscription_created &&
        command_exists(control_directory, "create_subscription")) {
      if (create_subscription(fixture, 0U, "graph_alpha") != 0) {
        return 1;
      }
      subscription_created = true;
      puts("GRAPH_FIXTURE_STATE publisher_subscription");
      fflush(stdout);
    }
    if (subscription_created && !publisher_destroyed &&
        command_exists(control_directory, "destroy_publisher")) {
      if (destroy_publisher(fixture, 0U) != 0) {
        return 1;
      }
      publisher_destroyed = true;
      puts("GRAPH_FIXTURE_STATE subscription");
      fflush(stdout);
    }
    if (publisher_destroyed && !subscription_destroyed &&
        command_exists(control_directory, "destroy_subscription")) {
      if (destroy_subscription(fixture, 0U) != 0) {
        return 1;
      }
      subscription_destroyed = true;
      puts("GRAPH_FIXTURE_STATE node");
      fflush(stdout);
    }
    if (command_exists(control_directory, "stop")) {
      return 0;
    }
    if (!pause_100_ms()) {
      return 1;
    }
  }
  fputs("GRAPH_FIXTURE_ERROR timeout\n", stderr);
  return 1;
}

int main(int argc, char **argv)
{
  if (argc < 3 || argc > 4) {
    fprintf(stderr, "usage: %s TOPOLOGY CONTROL_DIRECTORY [NAMESPACE]\n", argv[0]);
    return 2;
  }

  const char *topology = argv[1];
  const char *control_directory = argv[2];
  const char *namespace_ = argc == 4 ? argv[3] : "/";
  int result = 1;
  rcl_allocator_t allocator = rcl_get_default_allocator();
  rclc_support_t support;
  fixture_t fixture = {
      .node = rcl_get_zero_initialized_node(),
      .publishers = {rcl_get_zero_initialized_publisher(), rcl_get_zero_initialized_publisher()},
      .subscriptions = {rcl_get_zero_initialized_subscription(),
                        rcl_get_zero_initialized_subscription()},
  };

  if (check_rcl(rclc_support_init(&support, 0, NULL, &allocator), "support_init") != 0) {
    return result;
  }
  rcl_node_options_t options = rcl_node_get_default_options();
  options.enable_rosout = false;
  if (check_rcl(rclc_node_init_with_options(&fixture.node, "graph_fixture", namespace_, &support,
                                            &options),
                "node_init") != 0) {
    goto fini_support;
  }
  if (create_topology(&fixture, topology) != 0) {
    goto fini_endpoints;
  }

  printf("GRAPH_FIXTURE_READY topology=%s namespace=%s\n", topology, namespace_);
  fflush(stdout);
  if (strcmp(topology, "lifecycle") == 0) {
    result = run_lifecycle(&fixture, control_directory);
  } else {
    result = 1;
    for (unsigned int tick = 0U; tick < MAX_WAIT_TICKS; ++tick) {
      if (command_exists(control_directory, "stop")) {
        result = 0;
        break;
      }
      if (!pause_100_ms()) {
        break;
      }
    }
  }

fini_endpoints:
  for (size_t index = 0U; index < 2U; ++index) {
    if (destroy_subscription(&fixture, index) != 0) {
      result = 1;
    }
    if (destroy_publisher(&fixture, index) != 0) {
      result = 1;
    }
  }
  if (check_rcl(rcl_node_fini(&fixture.node), "node_fini") != 0) {
    result = 1;
  }
fini_support:
  if (check_rcl(rclc_support_fini(&support), "support_fini") != 0) {
    result = 1;
  }
  if (result == 0) {
    puts("GRAPH_FIXTURE_DONE");
  }
  return result;
}
