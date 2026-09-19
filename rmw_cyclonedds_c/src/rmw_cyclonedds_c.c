// SPDX-License-Identifier: Apache-2.0

#include <errno.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dds/dds.h>
#include <rcutils/allocator.h>
#include <rcutils/error_handling.h>
#include <rmw/discovery_options.h>
#include <rmw/enclave.h>
#include <rmw/error_handling.h>
#include <rmw/init.h>
#include <rmw/init_options.h>
#include <rmw/rmw.h>
#include <rmw/security_options.h>
#include <rmw/validate_namespace.h>
#include <rmw/validate_node_name.h>
#include <rosidl_runtime_c/message_type_support_struct.h>
#include <rosidl_typesupport_c/message_type_support_dispatch.h>
#ifdef RMW_CYCLONEDDS_C_HAS_GENERATED_TYPESUPPORT
#include <rosidl_typesupport_cyclonedds_c/identifier.h>
#include <rosidl_typesupport_cyclonedds_c/message_type_support.h>
#else
typedef bool (*rosidl_typesupport_cyclonedds_c__convert_fn)(const void *source, void *destination);
typedef struct rosidl_typesupport_cyclonedds_c__message_type_support_callbacks_s {
  const char *ros_type_name;
  const char *dds_type_name;
  const dds_topic_descriptor_t *descriptor;
  size_t ros_size;
  size_t dds_size;
  rosidl_typesupport_cyclonedds_c__convert_fn ros_to_dds;
  rosidl_typesupport_cyclonedds_c__convert_fn dds_to_ros;
} rosidl_typesupport_cyclonedds_c__message_type_support_callbacks_t;
#endif
#include <std_msgs/msg/u_int32.h>

#include "ParticipantEntitiesInfo_.h"
#include "UInt32_.h"

static const char implementation_identifier[] = "rmw_cyclonedds_c";
static const char serialization_format[] = "cdr";

#ifndef RMW_CYCLONEDDS_C_GRAPH_MAX_NODES
#define RMW_CYCLONEDDS_C_GRAPH_MAX_NODES 8U
#endif
#ifndef RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE
#define RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE 16U
#endif

enum { GRAPH_STRING_BOUND = 256U };

_Static_assert(RMW_GID_STORAGE_SIZE >= sizeof(dds_guid_t),
               "rmw_gid_t is too small for a Cyclone DDS GUID");

typedef struct node_data_s {
  rmw_context_impl_t *context;
  size_t graph_index;
} node_data_t;

typedef struct endpoint_data_s {
  dds_entity_t topic;
  dds_entity_t endpoint;
  dds_entity_t condition;
  dds_guid_t guid;
  node_data_t *node;
  rmw_qos_profile_t qos;
  const rosidl_typesupport_cyclonedds_c__message_type_support_callbacks_t *type_support;
  void *dds_sample;
  atomic_flag sample_lock;
} endpoint_data_t;

typedef struct wait_set_data_s {
  dds_entity_t waitset;
  dds_entity_t *attached;
  dds_attach_t *triggered;
  size_t capacity;
  size_t attached_count;
  rcutils_allocator_t allocator;
} wait_set_data_t;

typedef struct guard_condition_data_s {
  dds_entity_t entity;
  rcutils_allocator_t allocator;
} guard_condition_data_t;

struct rmw_context_impl_s {
  dds_entity_t domain;
  dds_entity_t participant;
  dds_entity_t graph_topic;
  dds_entity_t graph_writer;
  dds_entity_t graph_guard_entity;
  rmw_guard_condition_t graph_guard;
  dds_guid_t participant_guid;
  rmw_dds_common_msg_dds__NodeEntitiesInfo_
      graph_nodes[RMW_CYCLONEDDS_C_GRAPH_MAX_NODES];
  rmw_dds_common_msg_dds__Gid_
      graph_readers[RMW_CYCLONEDDS_C_GRAPH_MAX_NODES]
                   [RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE];
  rmw_dds_common_msg_dds__Gid_
      graph_writers[RMW_CYCLONEDDS_C_GRAPH_MAX_NODES]
                   [RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE];
  node_data_t *graph_node_owners[RMW_CYCLONEDDS_C_GRAPH_MAX_NODES];
  size_t graph_node_count;
  atomic_flag graph_lock;
  rcutils_allocator_t allocator;
  bool shutdown;
};

static bool identifiers_match(const char *identifier)
{
  return identifier != NULL && strcmp(identifier, implementation_identifier) == 0;
}

static bool allocator_is_valid(const rcutils_allocator_t *allocator)
{
  return allocator != NULL && rcutils_allocator_is_valid(allocator);
}

static void *allocate_zeroed(rcutils_allocator_t *allocator, size_t size)
{
  void *memory = allocator->zero_allocate(1U, size, allocator->state);
  if (memory == NULL) {
    RMW_SET_ERROR_MSG("allocation failed");
  }
  return memory;
}

static char *copy_string(rcutils_allocator_t *allocator, const char *source)
{
  const size_t length = strlen(source) + 1U;
  char *destination = allocator->allocate(length, allocator->state);
  if (destination == NULL) {
    RMW_SET_ERROR_MSG("string allocation failed");
    return NULL;
  }
  memcpy(destination, source, length);
  return destination;
}

static rmw_ret_t map_dds_result(dds_return_t result, const char *operation)
{
  if (result >= 0) {
    return RMW_RET_OK;
  }
  RMW_SET_ERROR_MSG_WITH_FORMAT_STRING("%s failed: %s (%" PRId32 ")", operation,
                                       dds_strretcode(-result), result);
  return RMW_RET_ERROR;
}

static void lock_graph(rmw_context_impl_t *context)
{
  while (atomic_flag_test_and_set_explicit(&context->graph_lock, memory_order_acquire)) {
  }
}

static void unlock_graph(rmw_context_impl_t *context)
{
  atomic_flag_clear_explicit(&context->graph_lock, memory_order_release);
}

static void graph_reset_node(rmw_context_impl_t *context, size_t index)
{
  rmw_dds_common_msg_dds__NodeEntitiesInfo_ *node = &context->graph_nodes[index];
  memset(node, 0, sizeof(*node));
  node->reader_gid_seq._maximum = RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE;
  node->reader_gid_seq._buffer = context->graph_readers[index];
  node->writer_gid_seq._maximum = RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE;
  node->writer_gid_seq._buffer = context->graph_writers[index];
  memset(context->graph_readers[index], 0, sizeof(context->graph_readers[index]));
  memset(context->graph_writers[index], 0, sizeof(context->graph_writers[index]));
  context->graph_node_owners[index] = NULL;
}

static rmw_ret_t graph_publish_locked(rmw_context_impl_t *context)
{
  rmw_dds_common_msg_dds__ParticipantEntitiesInfo_ sample;
  memset(&sample, 0, sizeof(sample));
  memcpy(sample.gid.data, context->participant_guid.v, sizeof(sample.gid.data));
  sample.node_entities_info_seq._maximum = RMW_CYCLONEDDS_C_GRAPH_MAX_NODES;
  sample.node_entities_info_seq._length = (uint32_t)context->graph_node_count;
  sample.node_entities_info_seq._buffer = context->graph_nodes;
  return map_dds_result(dds_write(context->graph_writer, &sample),
                        "dds_write graph announcement");
}

static bool graph_node_is_valid_locked(const rmw_context_impl_t *context,
                                       const node_data_t *node)
{
  return node != NULL && node->context == context && node->graph_index < context->graph_node_count &&
         context->graph_node_owners[node->graph_index] == node;
}

static rmw_ret_t graph_add_node(rmw_context_impl_t *context, node_data_t *node, const char *name,
                                const char *namespace_)
{
  if (strlen(name) > GRAPH_STRING_BOUND || strlen(namespace_) > GRAPH_STRING_BOUND) {
    RMW_SET_ERROR_MSG("node name and namespace must fit the graph protocol's 256-byte bounds");
    return RMW_RET_INVALID_ARGUMENT;
  }

  lock_graph(context);
  if (context->graph_node_count == RMW_CYCLONEDDS_C_GRAPH_MAX_NODES) {
    unlock_graph(context);
    RMW_SET_ERROR_MSG("local graph node limit reached");
    return RMW_RET_ERROR;
  }

  const size_t index = context->graph_node_count;
  graph_reset_node(context, index);
  rmw_dds_common_msg_dds__NodeEntitiesInfo_ *graph_node = &context->graph_nodes[index];
  memcpy(graph_node->node_name, name, strlen(name) + 1U);
  memcpy(graph_node->node_namespace, namespace_, strlen(namespace_) + 1U);
  node->context = context;
  node->graph_index = index;
  context->graph_node_owners[index] = node;
  ++context->graph_node_count;

  const rmw_ret_t result = graph_publish_locked(context);
  if (result != RMW_RET_OK) {
    --context->graph_node_count;
    graph_reset_node(context, index);
    node->context = NULL;
  }
  unlock_graph(context);
  return result;
}

static void graph_copy_node(rmw_context_impl_t *context, size_t destination, size_t source)
{
  const rmw_dds_common_msg_dds__NodeEntitiesInfo_ *source_node = &context->graph_nodes[source];
  const uint32_t reader_count = source_node->reader_gid_seq._length;
  const uint32_t writer_count = source_node->writer_gid_seq._length;
  node_data_t *owner = context->graph_node_owners[source];

  graph_reset_node(context, destination);
  rmw_dds_common_msg_dds__NodeEntitiesInfo_ *destination_node =
      &context->graph_nodes[destination];
  memcpy(destination_node->node_name, source_node->node_name,
         sizeof(destination_node->node_name));
  memcpy(destination_node->node_namespace, source_node->node_namespace,
         sizeof(destination_node->node_namespace));
  memcpy(context->graph_readers[destination], context->graph_readers[source],
         (size_t)reader_count * sizeof(context->graph_readers[destination][0]));
  memcpy(context->graph_writers[destination], context->graph_writers[source],
         (size_t)writer_count * sizeof(context->graph_writers[destination][0]));
  destination_node->reader_gid_seq._length = reader_count;
  destination_node->writer_gid_seq._length = writer_count;
  context->graph_node_owners[destination] = owner;
  owner->graph_index = destination;
}

static rmw_ret_t graph_remove_node(rmw_context_impl_t *context, node_data_t *node)
{
  lock_graph(context);
  if (!graph_node_is_valid_locked(context, node)) {
    unlock_graph(context);
    RMW_SET_ERROR_MSG("node is absent from the local graph");
    return RMW_RET_INVALID_ARGUMENT;
  }

  rmw_dds_common_msg_dds__NodeEntitiesInfo_ *graph_node =
      &context->graph_nodes[node->graph_index];
  if (graph_node->reader_gid_seq._length != 0U || graph_node->writer_gid_seq._length != 0U) {
    unlock_graph(context);
    RMW_SET_ERROR_MSG("node still owns graph endpoints");
    return RMW_RET_INVALID_ARGUMENT;
  }

  const size_t removed_index = node->graph_index;
  for (size_t index = removed_index; index + 1U < context->graph_node_count; ++index) {
    graph_copy_node(context, index, index + 1U);
  }
  --context->graph_node_count;
  graph_reset_node(context, context->graph_node_count);
  node->context = NULL;
  const rmw_ret_t result = graph_publish_locked(context);
  unlock_graph(context);
  return result;
}

static bool graph_gids_equal(const rmw_dds_common_msg_dds__Gid_ *left, const dds_guid_t *right)
{
  return memcmp(left->data, right->v, sizeof(left->data)) == 0;
}

static rmw_ret_t graph_add_endpoint(rmw_context_impl_t *context, node_data_t *node,
                                    const dds_guid_t *guid, bool writer)
{
  lock_graph(context);
  if (!graph_node_is_valid_locked(context, node)) {
    unlock_graph(context);
    RMW_SET_ERROR_MSG("endpoint node is absent from the local graph");
    return RMW_RET_INVALID_ARGUMENT;
  }

  rmw_dds_common_msg_dds__NodeEntitiesInfo_ *graph_node =
      &context->graph_nodes[node->graph_index];
  dds_sequence_rmw_dds_common_msg_dds__Gid_ *sequence =
      writer ? &graph_node->writer_gid_seq : &graph_node->reader_gid_seq;
  if (sequence->_length == RMW_CYCLONEDDS_C_GRAPH_MAX_ENDPOINTS_PER_NODE) {
    unlock_graph(context);
    RMW_SET_ERROR_MSG("local graph endpoint limit reached");
    return RMW_RET_ERROR;
  }

  rmw_dds_common_msg_dds__Gid_ *entry = &sequence->_buffer[sequence->_length];
  memcpy(entry->data, guid->v, sizeof(entry->data));
  ++sequence->_length;
  const rmw_ret_t result = graph_publish_locked(context);
  if (result != RMW_RET_OK) {
    --sequence->_length;
    memset(entry, 0, sizeof(*entry));
  }
  unlock_graph(context);
  return result;
}

static rmw_ret_t graph_remove_endpoint(rmw_context_impl_t *context, node_data_t *node,
                                       const dds_guid_t *guid, bool writer)
{
  lock_graph(context);
  if (!graph_node_is_valid_locked(context, node)) {
    unlock_graph(context);
    RMW_SET_ERROR_MSG("endpoint node is absent from the local graph");
    return RMW_RET_INVALID_ARGUMENT;
  }

  rmw_dds_common_msg_dds__NodeEntitiesInfo_ *graph_node =
      &context->graph_nodes[node->graph_index];
  dds_sequence_rmw_dds_common_msg_dds__Gid_ *sequence =
      writer ? &graph_node->writer_gid_seq : &graph_node->reader_gid_seq;
  uint32_t removed_index = sequence->_length;
  for (uint32_t index = 0U; index < sequence->_length; ++index) {
    if (graph_gids_equal(&sequence->_buffer[index], guid)) {
      removed_index = index;
      break;
    }
  }
  if (removed_index == sequence->_length) {
    unlock_graph(context);
    RMW_SET_ERROR_MSG("endpoint is absent from the local graph");
    return RMW_RET_INVALID_ARGUMENT;
  }

  for (uint32_t index = removed_index; index + 1U < sequence->_length; ++index) {
    sequence->_buffer[index] = sequence->_buffer[index + 1U];
  }
  --sequence->_length;
  memset(&sequence->_buffer[sequence->_length], 0, sizeof(sequence->_buffer[0]));
  const rmw_ret_t result = graph_publish_locked(context);
  unlock_graph(context);
  return result;
}

static rmw_ret_t create_graph_writer(rmw_context_impl_t *context)
{
  static const char graph_type_hash[] =
      "typehash=RIHS01_91a0593bacdcc50ea9bdcf849a938b128412cc1ea821245c663bcd26f83c295e;";
  const dds_return_t guid_result = dds_get_guid(context->participant, &context->participant_guid);
  if (guid_result < 0) {
    return map_dds_result(guid_result, "dds_get_guid participant");
  }

  context->graph_topic = dds_create_topic(
      context->participant, &rmw_dds_common_msg_dds__ParticipantEntitiesInfo__desc,
      "ros_discovery_info", NULL, NULL);
  if (context->graph_topic < 0) {
    return map_dds_result(context->graph_topic, "dds_create_topic graph");
  }

  dds_qos_t *qos = dds_create_qos();
  if (qos == NULL) {
    RMW_SET_ERROR_MSG("dds_create_qos failed for graph writer");
    return RMW_RET_ERROR;
  }
  dds_qset_history(qos, DDS_HISTORY_KEEP_LAST, 1);
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_durability(qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_durability_service(qos, 0, DDS_HISTORY_KEEP_LAST, 1, DDS_LENGTH_UNLIMITED,
                              DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);
  dds_qset_writer_data_lifecycle(qos, false);
  dds_qset_userdata(qos, graph_type_hash, strlen(graph_type_hash));
  const dds_data_representation_id_t representation = DDS_DATA_REPRESENTATION_XCDR1;
  dds_qset_data_representation(qos, 1U, &representation);
  context->graph_writer = dds_create_writer(context->participant, context->graph_topic, qos, NULL);
  dds_delete_qos(qos);
  if (context->graph_writer < 0) {
    return map_dds_result(context->graph_writer, "dds_create_writer graph");
  }
  return RMW_RET_OK;
}

static bool legacy_uint32_ros_to_dds(const void *ros_message, void *dds_message)
{
  if (ros_message == NULL || dds_message == NULL) {
    return false;
  }
  ((std_msgs_msg_dds__UInt32_ *)dds_message)->data_ =
      ((const std_msgs__msg__UInt32 *)ros_message)->data;
  return true;
}

static bool legacy_uint32_dds_to_ros(const void *dds_message, void *ros_message)
{
  if (ros_message == NULL || dds_message == NULL) {
    return false;
  }
  ((std_msgs__msg__UInt32 *)ros_message)->data =
      ((const std_msgs_msg_dds__UInt32_ *)dds_message)->data_;
  return true;
}

static const rosidl_typesupport_cyclonedds_c__message_type_support_callbacks_t
    legacy_uint32_type_support = {
        "std_msgs/msg/UInt32",
        "std_msgs::msg::dds_::UInt32_",
        &std_msgs_msg_dds__UInt32__desc,
        sizeof(std_msgs__msg__UInt32),
        sizeof(std_msgs_msg_dds__UInt32_),
        &legacy_uint32_ros_to_dds,
        &legacy_uint32_dds_to_ros,
};

static const rosidl_typesupport_cyclonedds_c__message_type_support_callbacks_t *
resolve_type_support(const rosidl_message_type_support_t *type_support)
{
  if (type_support == NULL) {
    RMW_SET_ERROR_MSG("message type support is null");
    return NULL;
  }
#ifdef RMW_CYCLONEDDS_C_HAS_GENERATED_TYPESUPPORT
  const rosidl_message_type_support_t *cyclonedds_support =
      rosidl_typesupport_c__get_message_typesupport_handle_function(
          type_support, rosidl_typesupport_cyclonedds_c__identifier);
  if (cyclonedds_support != NULL && cyclonedds_support->data != NULL) {
    rcutils_reset_error();
    return cyclonedds_support->data;
  }
  rcutils_reset_error();
#endif
  if (type_support->get_type_description_func == NULL) {
    RMW_SET_ERROR_MSG("message type support has no type description");
    return NULL;
  }
  const rosidl_runtime_c__type_description__TypeDescription *description =
      type_support->get_type_description_func(type_support);
  if (description != NULL && description->type_description.type_name.data != NULL &&
      strcmp(description->type_description.type_name.data, "std_msgs/msg/UInt32") == 0) {
    return &legacy_uint32_type_support;
  }
  RMW_SET_ERROR_MSG("type has no generated rosidl_typesupport_cyclonedds_c fixed-size adapter");
  return NULL;
}

static void lock_sample(endpoint_data_t *endpoint)
{
  while (atomic_flag_test_and_set_explicit(&endpoint->sample_lock, memory_order_acquire)) {
  }
}

static void unlock_sample(endpoint_data_t *endpoint)
{
  atomic_flag_clear_explicit(&endpoint->sample_lock, memory_order_release);
}

static bool qos_is_supported(const rmw_qos_profile_t *qos)
{
  if (qos == NULL) {
    RMW_SET_ERROR_MSG("QoS profile is null");
    return false;
  }
  if (qos->history != RMW_QOS_POLICY_HISTORY_KEEP_LAST || qos->depth == 0U ||
      qos->depth > (size_t)INT32_MAX ||
      (qos->reliability != RMW_QOS_POLICY_RELIABILITY_BEST_EFFORT &&
       qos->reliability != RMW_QOS_POLICY_RELIABILITY_RELIABLE) ||
      (qos->durability != RMW_QOS_POLICY_DURABILITY_VOLATILE &&
       qos->durability != RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL) ||
      qos->avoid_ros_namespace_conventions) {
    RMW_SET_ERROR_MSG(
        "The supported profile accepts ROS naming, best-effort or reliable, volatile or "
        "transient-local, finite keep-last QoS only");
    return false;
  }
  return true;
}

static bool set_type_hash_user_data(dds_qos_t *dds_qos,
                                    const rosidl_message_type_support_t *type_support)
{
  if (type_support->get_type_hash_func == NULL) {
    RMW_SET_ERROR_MSG("message type support has no type hash");
    return false;
  }
  const rosidl_type_hash_t *type_hash = type_support->get_type_hash_func(type_support);
  if (type_hash == NULL || type_hash->version == ROSIDL_TYPE_HASH_VERSION_UNSET) {
    RMW_SET_ERROR_MSG("message type hash is unset");
    return false;
  }
  char user_data[82];
  const int prefix_length = snprintf(user_data, sizeof(user_data), "typehash=RIHS%02u_",
                                     (unsigned int)type_hash->version);
  if (prefix_length != 16) {
    RMW_SET_ERROR_MSG("message type hash version cannot be encoded");
    return false;
  }
  static const char hex[] = "0123456789abcdef";
  size_t offset = (size_t)prefix_length;
  for (size_t index = 0U; index < ROSIDL_TYPE_HASH_SIZE; ++index) {
    user_data[offset++] = hex[type_hash->value[index] >> 4U];
    user_data[offset++] = hex[type_hash->value[index] & 0x0fU];
  }
  user_data[offset++] = ';';
  user_data[offset] = '\0';
  dds_qset_userdata(dds_qos, user_data, offset);
  return true;
}

static dds_qos_t *create_dds_qos(const rmw_qos_profile_t *qos, bool writer,
                                 const rosidl_message_type_support_t *type_support)
{
  dds_qos_t *dds_qos = dds_create_qos();
  if (dds_qos == NULL) {
    RMW_SET_ERROR_MSG("dds_create_qos failed");
    return NULL;
  }
  dds_qset_history(dds_qos, DDS_HISTORY_KEEP_LAST, (int32_t)qos->depth);
  if (qos->reliability == RMW_QOS_POLICY_RELIABILITY_RELIABLE) {
    dds_qset_reliability(dds_qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  } else {
    dds_qset_reliability(dds_qos, DDS_RELIABILITY_BEST_EFFORT, DDS_MSECS(0));
  }
  dds_qset_durability(dds_qos, qos->durability == RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL
                                   ? DDS_DURABILITY_TRANSIENT_LOCAL
                                   : DDS_DURABILITY_VOLATILE);
  if (writer && qos->durability == RMW_QOS_POLICY_DURABILITY_TRANSIENT_LOCAL) {
    dds_qset_durability_service(dds_qos, 0, DDS_HISTORY_KEEP_LAST, (int32_t)qos->depth,
                                DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED,
                                DDS_LENGTH_UNLIMITED);
  }
  if (!set_type_hash_user_data(dds_qos, type_support)) {
    dds_delete_qos(dds_qos);
    return NULL;
  }
  return dds_qos;
}

static char *make_dds_topic_name(rcutils_allocator_t *allocator, const char *ros_topic)
{
  if (ros_topic == NULL || ros_topic[0] != '/') {
    RMW_SET_ERROR_MSG("topic name must be absolute");
    return NULL;
  }
  const size_t ros_length = strlen(ros_topic);
  if (ros_length > SIZE_MAX - 3U) {
    RMW_SET_ERROR_MSG("topic name length overflow");
    return NULL;
  }
  char *dds_topic = allocator->allocate(ros_length + 3U, allocator->state);
  if (dds_topic == NULL) {
    RMW_SET_ERROR_MSG("DDS topic name allocation failed");
    return NULL;
  }
  memcpy(dds_topic, "rt", 2U);
  memcpy(dds_topic + 2U, ros_topic, ros_length + 1U);
  return dds_topic;
}

static rmw_context_impl_t *context_impl_from_node(const rmw_node_t *node)
{
  if (node == NULL || !identifiers_match(node->implementation_identifier) ||
      node->context == NULL || !identifiers_match(node->context->implementation_identifier) ||
      node->context->impl == NULL) {
    RMW_SET_ERROR_MSG("invalid node or RMW implementation identifier");
    return NULL;
  }
  return node->context->impl;
}

const char *rmw_get_implementation_identifier(void) { return implementation_identifier; }

const char *rmw_get_serialization_format(void) { return serialization_format; }

rmw_ret_t rmw_init_options_init(rmw_init_options_t *init_options, rcutils_allocator_t allocator)
{
  if (init_options == NULL || !allocator_is_valid(&allocator) ||
      init_options->implementation_identifier != NULL) {
    RMW_SET_ERROR_MSG("invalid or already initialized init options");
    return RMW_RET_INVALID_ARGUMENT;
  }

  rmw_init_options_t initialized = rmw_get_zero_initialized_init_options();
  initialized.implementation_identifier = implementation_identifier;
  initialized.domain_id = RMW_DEFAULT_DOMAIN_ID;
  initialized.security_options = rmw_get_zero_initialized_security_options();
  initialized.discovery_options = rmw_get_zero_initialized_discovery_options();
  initialized.allocator = allocator;
  rmw_ret_t result =
      rmw_discovery_options_init(&initialized.discovery_options, 0U, &initialized.allocator);
  if (result != RMW_RET_OK) {
    return result;
  }
  initialized.discovery_options.automatic_discovery_range =
      RMW_AUTOMATIC_DISCOVERY_RANGE_SYSTEM_DEFAULT;
  *init_options = initialized;
  return RMW_RET_OK;
}

rmw_ret_t rmw_init_options_copy(const rmw_init_options_t *source, rmw_init_options_t *destination)
{
  if (source == NULL || destination == NULL || source == destination ||
      source->implementation_identifier == NULL || destination->implementation_identifier != NULL) {
    RMW_SET_ERROR_MSG("invalid init options copy arguments");
    return RMW_RET_INVALID_ARGUMENT;
  }
  if (!identifiers_match(source->implementation_identifier)) {
    RMW_SET_ERROR_MSG("incorrect RMW implementation for source init options");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  rmw_init_options_t copy = rmw_get_zero_initialized_init_options();
  rmw_ret_t result = rmw_init_options_init(&copy, source->allocator);
  if (result != RMW_RET_OK) {
    return result;
  }
  copy.instance_id = source->instance_id;
  copy.domain_id = source->domain_id;

  result = rmw_discovery_options_fini(&copy.discovery_options);
  if (result == RMW_RET_OK) {
    result = rmw_discovery_options_copy(&source->discovery_options, &copy.allocator,
                                        &copy.discovery_options);
  }
  if (result == RMW_RET_OK) {
    result = rmw_security_options_copy(&source->security_options, &copy.allocator,
                                       &copy.security_options);
  }
  if (result == RMW_RET_OK && source->enclave != NULL) {
    result = rmw_enclave_options_copy(source->enclave, &copy.allocator, &copy.enclave);
  }
  if (result != RMW_RET_OK) {
    const rmw_ret_t cleanup_result = rmw_init_options_fini(&copy);
    (void)cleanup_result;
    return result;
  }
  *destination = copy;
  return RMW_RET_OK;
}

rmw_ret_t rmw_init_options_fini(rmw_init_options_t *init_options)
{
  if (init_options == NULL) {
    RMW_SET_ERROR_MSG("init options are null");
    return RMW_RET_INVALID_ARGUMENT;
  }
  if (init_options->implementation_identifier == NULL) {
    RMW_SET_ERROR_MSG("init options are not initialized");
    return RMW_RET_INVALID_ARGUMENT;
  }
  if (!identifiers_match(init_options->implementation_identifier)) {
    RMW_SET_ERROR_MSG("incorrect RMW implementation for init options");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  rmw_ret_t result = rmw_discovery_options_fini(&init_options->discovery_options);
  const rmw_ret_t security_result =
      rmw_security_options_fini(&init_options->security_options, &init_options->allocator);
  if (security_result != RMW_RET_OK) {
    result = security_result;
  }
  if (init_options->enclave != NULL) {
    const rmw_ret_t enclave_result =
        rmw_enclave_options_fini(init_options->enclave, &init_options->allocator);
    if (enclave_result != RMW_RET_OK) {
      result = enclave_result;
    }
  }
  *init_options = rmw_get_zero_initialized_init_options();
  return result;
}

static bool get_domain_id(const rmw_init_options_t *options, size_t *domain_id)
{
  if (options->domain_id != RMW_DEFAULT_DOMAIN_ID) {
    *domain_id = options->domain_id;
    return true;
  }
  const char *environment = getenv("ROS_DOMAIN_ID");
  if (environment == NULL || environment[0] == '\0') {
#ifdef RMW_CYCLONEDDS_C_DEFAULT_DOMAIN_ID
    *domain_id = (size_t)RMW_CYCLONEDDS_C_DEFAULT_DOMAIN_ID;
#else
    *domain_id = 0U;
#endif
    return true;
  }
  errno = 0;
  char *end = NULL;
  const unsigned long parsed = strtoul(environment, &end, 10);
  if (errno != 0 || end == environment || *end != '\0' || parsed > UINT32_MAX) {
    RMW_SET_ERROR_MSG("invalid ROS_DOMAIN_ID");
    return false;
  }
  *domain_id = (size_t)parsed;
  return true;
}

rmw_ret_t rmw_init(const rmw_init_options_t *options, rmw_context_t *context)
{
  if (options == NULL || context == NULL || options->implementation_identifier == NULL ||
      context->impl != NULL || !allocator_is_valid(&options->allocator) || options->enclave == NULL) {
    RMW_SET_ERROR_MSG("invalid RMW initialization arguments");
    return RMW_RET_INVALID_ARGUMENT;
  }
  if (!identifiers_match(options->implementation_identifier)) {
    RMW_SET_ERROR_MSG("incorrect RMW implementation for init options");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  size_t domain_id = 0U;
  if (!get_domain_id(options, &domain_id) || domain_id > UINT32_MAX) {
    return RMW_RET_INVALID_ARGUMENT;
  }

  rcutils_allocator_t allocator = options->allocator;
  rmw_context_impl_t *implementation = allocate_zeroed(&allocator, sizeof(*implementation));
  if (implementation == NULL) {
    return RMW_RET_BAD_ALLOC;
  }
  implementation->allocator = allocator;
  atomic_flag_clear(&implementation->graph_lock);
  for (size_t index = 0U; index < RMW_CYCLONEDDS_C_GRAPH_MAX_NODES; ++index) {
    graph_reset_node(implementation, index);
  }
  const char *cyclone_uri = getenv("CYCLONEDDS_URI");
#ifdef RMW_CYCLONEDDS_C_URI
  if (cyclone_uri == NULL || cyclone_uri[0] == '\0') {
    cyclone_uri = RMW_CYCLONEDDS_C_URI;
  }
#endif
  implementation->domain = dds_create_domain((dds_domainid_t)domain_id, cyclone_uri);
  if (implementation->domain < 0) {
    (void)map_dds_result(implementation->domain, "dds_create_domain");
    allocator.deallocate(implementation, allocator.state);
    return RMW_RET_ERROR;
  }
  implementation->participant = dds_create_participant((dds_domainid_t)domain_id, NULL, NULL);
  if (implementation->participant < 0) {
    (void)map_dds_result(implementation->participant, "dds_create_participant");
    (void)dds_delete(implementation->domain);
    allocator.deallocate(implementation, allocator.state);
    return RMW_RET_ERROR;
  }
  if (create_graph_writer(implementation) != RMW_RET_OK) {
    (void)dds_delete(implementation->participant);
    (void)dds_delete(implementation->domain);
    allocator.deallocate(implementation, allocator.state);
    return RMW_RET_ERROR;
  }
  implementation->graph_guard_entity = dds_create_guardcondition(implementation->participant);
  if (implementation->graph_guard_entity < 0) {
    (void)map_dds_result(implementation->graph_guard_entity, "dds_create_guardcondition");
    (void)dds_delete(implementation->participant);
    (void)dds_delete(implementation->domain);
    allocator.deallocate(implementation, allocator.state);
    return RMW_RET_ERROR;
  }
  implementation->graph_guard.implementation_identifier = implementation_identifier;
  implementation->graph_guard.data = &implementation->graph_guard_entity;
  implementation->graph_guard.context = context;

  rmw_context_t initialized = rmw_get_zero_initialized_context();
  initialized.instance_id = options->instance_id;
  initialized.implementation_identifier = implementation_identifier;
  initialized.actual_domain_id = domain_id;
  initialized.impl = implementation;
  const rmw_ret_t copy_result = rmw_init_options_copy(options, &initialized.options);
  if (copy_result != RMW_RET_OK) {
    (void)dds_delete(implementation->participant);
    (void)dds_delete(implementation->domain);
    allocator.deallocate(implementation, allocator.state);
    return copy_result;
  }
  *context = initialized;
  implementation->graph_guard.context = context;
  return RMW_RET_OK;
}

rmw_ret_t rmw_shutdown(rmw_context_t *context)
{
  if (context == NULL || context->implementation_identifier == NULL || context->impl == NULL) {
    RMW_SET_ERROR_MSG("invalid context for shutdown");
    return RMW_RET_INVALID_ARGUMENT;
  }
  if (!identifiers_match(context->implementation_identifier)) {
    RMW_SET_ERROR_MSG("incorrect RMW implementation for context");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }
  context->impl->shutdown = true;
  return map_dds_result(dds_set_guardcondition(context->impl->graph_guard_entity, true),
                        "dds_set_guardcondition");
}

rmw_ret_t rmw_context_fini(rmw_context_t *context)
{
  if (context == NULL || context->implementation_identifier == NULL || context->impl == NULL) {
    RMW_SET_ERROR_MSG("invalid context for finalization");
    return RMW_RET_INVALID_ARGUMENT;
  }
  if (!identifiers_match(context->implementation_identifier)) {
    RMW_SET_ERROR_MSG("incorrect RMW implementation for context");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }
  if (!context->impl->shutdown) {
    RMW_SET_ERROR_MSG("context must be shut down before finalization");
    return RMW_RET_INVALID_ARGUMENT;
  }

  rmw_context_impl_t *implementation = context->impl;
  rcutils_allocator_t allocator = implementation->allocator;
  rmw_ret_t result =
      map_dds_result(dds_delete(implementation->participant), "dds_delete participant");
  const rmw_ret_t domain_result =
      map_dds_result(dds_delete(implementation->domain), "dds_delete domain");
  if (domain_result != RMW_RET_OK) {
    result = domain_result;
  }
  const rmw_ret_t options_result = rmw_init_options_fini(&context->options);
  if (options_result != RMW_RET_OK) {
    result = options_result;
  }
  allocator.deallocate(implementation, allocator.state);
  *context = rmw_get_zero_initialized_context();
  return result;
}

rmw_node_t *rmw_create_node(rmw_context_t *context, const char *name, const char *namespace_)
{
  if (context == NULL || !identifiers_match(context->implementation_identifier) ||
      context->impl == NULL || context->impl->shutdown || name == NULL || namespace_ == NULL) {
    RMW_SET_ERROR_MSG("invalid node creation arguments");
    return NULL;
  }
  int validation_result = 0;
  if (rmw_validate_node_name(name, &validation_result, NULL) != RMW_RET_OK ||
      validation_result != RMW_NODE_NAME_VALID) {
    RMW_SET_ERROR_MSG("invalid node name");
    return NULL;
  }
  if (rmw_validate_namespace(namespace_, &validation_result, NULL) != RMW_RET_OK ||
      validation_result != RMW_NAMESPACE_VALID) {
    RMW_SET_ERROR_MSG("invalid node namespace");
    return NULL;
  }
  rcutils_allocator_t *allocator = &context->impl->allocator;
  rmw_node_t *node = allocate_zeroed(allocator, sizeof(*node));
  node_data_t *node_data = allocate_zeroed(allocator, sizeof(*node_data));
  if (node == NULL || node_data == NULL) {
    if (node != NULL) {
      allocator->deallocate(node, allocator->state);
    }
    if (node_data != NULL) {
      allocator->deallocate(node_data, allocator->state);
    }
    return NULL;
  }
  node->name = copy_string(allocator, name);
  node->namespace_ = copy_string(allocator, namespace_);
  if (node->name == NULL || node->namespace_ == NULL) {
    if (node->name != NULL) {
      allocator->deallocate((void *)node->name, allocator->state);
    }
    if (node->namespace_ != NULL) {
      allocator->deallocate((void *)node->namespace_, allocator->state);
    }
    allocator->deallocate(node_data, allocator->state);
    allocator->deallocate(node, allocator->state);
    return NULL;
  }
  node->implementation_identifier = implementation_identifier;
  node->data = node_data;
  node->context = context;
  if (graph_add_node(context->impl, node_data, node->name, node->namespace_) != RMW_RET_OK) {
    allocator->deallocate((void *)node->name, allocator->state);
    allocator->deallocate((void *)node->namespace_, allocator->state);
    allocator->deallocate(node_data, allocator->state);
    allocator->deallocate(node, allocator->state);
    return NULL;
  }
  (void)dds_set_guardcondition(context->impl->graph_guard_entity, true);
  return node;
}

rmw_ret_t rmw_destroy_node(rmw_node_t *node)
{
  if (node == NULL || node->implementation_identifier == NULL) {
    RMW_SET_ERROR_MSG("invalid node");
    return RMW_RET_INVALID_ARGUMENT;
  }
  if (!identifiers_match(node->implementation_identifier)) {
    RMW_SET_ERROR_MSG("incorrect RMW implementation for node");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }
  rmw_context_impl_t *implementation = context_impl_from_node(node);
  if (implementation == NULL) {
    return RMW_RET_INVALID_ARGUMENT;
  }
  node_data_t *node_data = node->data;
  if (node_data == NULL) {
    RMW_SET_ERROR_MSG("node has no local graph state");
    return RMW_RET_INVALID_ARGUMENT;
  }
  const rmw_ret_t result = graph_remove_node(implementation, node_data);
  if (result == RMW_RET_INVALID_ARGUMENT) {
    return result;
  }
  rcutils_allocator_t *allocator = &implementation->allocator;
  allocator->deallocate((void *)node->name, allocator->state);
  allocator->deallocate((void *)node->namespace_, allocator->state);
  allocator->deallocate(node_data, allocator->state);
  allocator->deallocate(node, allocator->state);
  (void)dds_set_guardcondition(implementation->graph_guard_entity, true);
  return result;
}

const rmw_guard_condition_t *rmw_node_get_graph_guard_condition(const rmw_node_t *node)
{
  rmw_context_impl_t *implementation = context_impl_from_node(node);
  return implementation == NULL ? NULL : &implementation->graph_guard;
}

rmw_guard_condition_t *rmw_create_guard_condition(rmw_context_t *context)
{
  if (context == NULL || !identifiers_match(context->implementation_identifier) ||
      context->impl == NULL || context->impl->shutdown) {
    RMW_SET_ERROR_MSG("invalid context for guard condition");
    return NULL;
  }
  rcutils_allocator_t *allocator = &context->impl->allocator;
  rmw_guard_condition_t *guard_condition = allocate_zeroed(allocator, sizeof(*guard_condition));
  guard_condition_data_t *data = allocate_zeroed(allocator, sizeof(*data));
  if (guard_condition == NULL || data == NULL) {
    goto fail;
  }
  data->allocator = *allocator;
  data->entity = dds_create_guardcondition(context->impl->participant);
  if (data->entity < 0) {
    (void)map_dds_result(data->entity, "dds_create_guardcondition");
    goto fail;
  }
  guard_condition->implementation_identifier = implementation_identifier;
  guard_condition->data = data;
  guard_condition->context = context;
  return guard_condition;

fail:
  if (data != NULL) {
    allocator->deallocate(data, allocator->state);
  }
  if (guard_condition != NULL) {
    allocator->deallocate(guard_condition, allocator->state);
  }
  return NULL;
}

rmw_ret_t rmw_destroy_guard_condition(rmw_guard_condition_t *guard_condition)
{
  if (guard_condition == NULL || !identifiers_match(guard_condition->implementation_identifier) ||
      guard_condition->data == NULL) {
    RMW_SET_ERROR_MSG("invalid guard condition destruction arguments");
    return RMW_RET_INVALID_ARGUMENT;
  }
  guard_condition_data_t *data = guard_condition->data;
  rcutils_allocator_t allocator = data->allocator;
  const rmw_ret_t result = map_dds_result(dds_delete(data->entity), "dds_delete guard condition");
  allocator.deallocate(data, allocator.state);
  allocator.deallocate(guard_condition, allocator.state);
  return result;
}

static rmw_publisher_t *create_publisher_handle(rmw_context_impl_t *context, const char *topic_name,
                                                const rmw_publisher_options_t *options)
{
  rcutils_allocator_t *allocator = &context->allocator;
  rmw_publisher_t *publisher = allocate_zeroed(allocator, sizeof(*publisher));
  if (publisher == NULL) {
    return NULL;
  }
  publisher->topic_name = copy_string(allocator, topic_name);
  if (publisher->topic_name == NULL) {
    allocator->deallocate(publisher, allocator->state);
    return NULL;
  }
  publisher->implementation_identifier = implementation_identifier;
  publisher->options = *options;
  publisher->can_loan_messages = false;
  return publisher;
}

rmw_publisher_t *rmw_create_publisher(const rmw_node_t *node,
                                      const rosidl_message_type_support_t *type_support,
                                      const char *topic_name, const rmw_qos_profile_t *qos,
                                      const rmw_publisher_options_t *options)
{
  rmw_context_impl_t *context = context_impl_from_node(node);
  node_data_t *node_data = node == NULL ? NULL : node->data;
  const rosidl_typesupport_cyclonedds_c__message_type_support_callbacks_t *callbacks =
      resolve_type_support(type_support);
  if (context == NULL || topic_name == NULL || options == NULL ||
      options->rmw_specific_publisher_payload != NULL ||
      options->require_unique_network_flow_endpoints !=
          RMW_UNIQUE_NETWORK_FLOW_ENDPOINTS_NOT_REQUIRED ||
      node_data == NULL || callbacks == NULL || !qos_is_supported(qos)) {
    return NULL;
  }

  rmw_publisher_t *publisher = create_publisher_handle(context, topic_name, options);
  if (publisher == NULL) {
    return NULL;
  }
  endpoint_data_t *endpoint = allocate_zeroed(&context->allocator, sizeof(*endpoint));
  char *dds_topic_name = make_dds_topic_name(&context->allocator, topic_name);
  dds_qos_t *dds_qos = create_dds_qos(qos, true, type_support);
  if (endpoint == NULL || dds_topic_name == NULL || dds_qos == NULL) {
    goto fail;
  }
  endpoint->type_support = callbacks;
  endpoint->dds_sample = allocate_zeroed(&context->allocator, callbacks->dds_size);
  atomic_flag_clear(&endpoint->sample_lock);
  if (endpoint->dds_sample == NULL) {
    goto fail;
  }
  endpoint->qos = *qos;
  endpoint->topic =
      dds_create_topic(context->participant, callbacks->descriptor, dds_topic_name, NULL, NULL);
  if (endpoint->topic < 0) {
    (void)map_dds_result(endpoint->topic, "dds_create_topic");
    goto fail;
  }
  endpoint->endpoint = dds_create_writer(context->participant, endpoint->topic, dds_qos, NULL);
  if (endpoint->endpoint < 0) {
    (void)map_dds_result(endpoint->endpoint, "dds_create_writer");
    goto fail;
  }
  const dds_return_t guid_result = dds_get_guid(endpoint->endpoint, &endpoint->guid);
  if (guid_result < 0) {
    (void)map_dds_result(guid_result, "dds_get_guid writer");
    goto fail;
  }
  endpoint->node = node_data;
  if (graph_add_endpoint(context, node_data, &endpoint->guid, true) != RMW_RET_OK) {
    goto fail;
  }
  dds_delete_qos(dds_qos);
  context->allocator.deallocate(dds_topic_name, context->allocator.state);
  publisher->data = endpoint;
  (void)dds_set_guardcondition(context->graph_guard_entity, true);
  return publisher;

fail:
  if (dds_qos != NULL) {
    dds_delete_qos(dds_qos);
  }
  if (dds_topic_name != NULL) {
    context->allocator.deallocate(dds_topic_name, context->allocator.state);
  }
  if (endpoint != NULL) {
    if (endpoint->endpoint > 0) {
      (void)dds_delete(endpoint->endpoint);
    }
    if (endpoint->topic > 0) {
      (void)dds_delete(endpoint->topic);
    }
    if (endpoint->dds_sample != NULL) {
      context->allocator.deallocate(endpoint->dds_sample, context->allocator.state);
    }
    context->allocator.deallocate(endpoint, context->allocator.state);
  }
  context->allocator.deallocate((void *)publisher->topic_name, context->allocator.state);
  context->allocator.deallocate(publisher, context->allocator.state);
  return NULL;
}

rmw_ret_t rmw_destroy_publisher(rmw_node_t *node, rmw_publisher_t *publisher)
{
  rmw_context_impl_t *context = context_impl_from_node(node);
  if (context == NULL || publisher == NULL ||
      !identifiers_match(publisher->implementation_identifier) || publisher->data == NULL) {
    RMW_SET_ERROR_MSG("invalid publisher destruction arguments");
    return RMW_RET_INVALID_ARGUMENT;
  }
  endpoint_data_t *endpoint = publisher->data;
  rmw_ret_t result = graph_remove_endpoint(context, endpoint->node, &endpoint->guid, true);
  const rmw_ret_t writer_result =
      map_dds_result(dds_delete(endpoint->endpoint), "dds_delete writer");
  const rmw_ret_t topic_result = map_dds_result(dds_delete(endpoint->topic), "dds_delete topic");
  if (writer_result != RMW_RET_OK || topic_result != RMW_RET_OK) {
    result = RMW_RET_ERROR;
  }
  context->allocator.deallocate(endpoint->dds_sample, context->allocator.state);
  context->allocator.deallocate(endpoint, context->allocator.state);
  context->allocator.deallocate((void *)publisher->topic_name, context->allocator.state);
  context->allocator.deallocate(publisher, context->allocator.state);
  (void)dds_set_guardcondition(context->graph_guard_entity, true);
  return result;
}

rmw_ret_t rmw_publisher_get_actual_qos(const rmw_publisher_t *publisher, rmw_qos_profile_t *qos)
{
  if (publisher == NULL || qos == NULL ||
      !identifiers_match(publisher->implementation_identifier) || publisher->data == NULL) {
    RMW_SET_ERROR_MSG("invalid publisher QoS query arguments");
    return RMW_RET_INVALID_ARGUMENT;
  }
  *qos = ((const endpoint_data_t *)publisher->data)->qos;
  return RMW_RET_OK;
}

rmw_ret_t rmw_get_gid_for_publisher(const rmw_publisher_t *publisher, rmw_gid_t *gid)
{
  if (publisher == NULL || gid == NULL ||
      !identifiers_match(publisher->implementation_identifier) || publisher->data == NULL) {
    RMW_SET_ERROR_MSG("invalid publisher GID query arguments");
    return RMW_RET_INVALID_ARGUMENT;
  }
  const endpoint_data_t *endpoint = publisher->data;
  memset(gid, 0, sizeof(*gid));
  gid->implementation_identifier = implementation_identifier;
  memcpy(gid->data, endpoint->guid.v, sizeof(endpoint->guid.v));
  return RMW_RET_OK;
}

rmw_ret_t rmw_publisher_count_matched_subscriptions(const rmw_publisher_t *publisher,
                                                    size_t *subscription_count)
{
  if (publisher == NULL || subscription_count == NULL ||
      !identifiers_match(publisher->implementation_identifier) || publisher->data == NULL) {
    RMW_SET_ERROR_MSG("invalid publisher match-count arguments");
    return RMW_RET_INVALID_ARGUMENT;
  }
  const endpoint_data_t *endpoint = publisher->data;
  const dds_return_t count = dds_get_matched_subscriptions(endpoint->endpoint, NULL, 0U);
  if (count < 0) {
    return map_dds_result(count, "dds_get_matched_subscriptions");
  }
  *subscription_count = (size_t)count;
  return RMW_RET_OK;
}

rmw_ret_t rmw_publish(const rmw_publisher_t *publisher, const void *ros_message,
                      rmw_publisher_allocation_t *allocation)
{
  if (publisher == NULL || ros_message == NULL ||
      !identifiers_match(publisher->implementation_identifier) || publisher->data == NULL) {
    RMW_SET_ERROR_MSG("invalid publish arguments");
    return RMW_RET_INVALID_ARGUMENT;
  }
  if (allocation != NULL) {
    RMW_SET_ERROR_MSG("publisher allocations are not supported");
    return RMW_RET_UNSUPPORTED;
  }
  endpoint_data_t *endpoint = publisher->data;
  lock_sample(endpoint);
  memset(endpoint->dds_sample, 0, endpoint->type_support->dds_size);
  if (!endpoint->type_support->ros_to_dds(ros_message, endpoint->dds_sample)) {
    unlock_sample(endpoint);
    RMW_SET_ERROR_MSG("generated ROS-to-DDS conversion failed");
    return RMW_RET_ERROR;
  }
  const rmw_ret_t result =
      map_dds_result(dds_write(endpoint->endpoint, endpoint->dds_sample), "dds_write");
  unlock_sample(endpoint);
  return result;
}

static rmw_subscription_t *create_subscription_handle(rmw_context_impl_t *context,
                                                      const char *topic_name,
                                                      const rmw_subscription_options_t *options)
{
  rcutils_allocator_t *allocator = &context->allocator;
  rmw_subscription_t *subscription = allocate_zeroed(allocator, sizeof(*subscription));
  if (subscription == NULL) {
    return NULL;
  }
  subscription->topic_name = copy_string(allocator, topic_name);
  if (subscription->topic_name == NULL) {
    allocator->deallocate(subscription, allocator->state);
    return NULL;
  }
  subscription->implementation_identifier = implementation_identifier;
  subscription->options = *options;
  subscription->can_loan_messages = false;
  subscription->is_cft_enabled = false;
  return subscription;
}

rmw_subscription_t *rmw_create_subscription(const rmw_node_t *node,
                                            const rosidl_message_type_support_t *type_support,
                                            const char *topic_name, const rmw_qos_profile_t *qos,
                                            const rmw_subscription_options_t *options)
{
  rmw_context_impl_t *context = context_impl_from_node(node);
  node_data_t *node_data = node == NULL ? NULL : node->data;
  const rosidl_typesupport_cyclonedds_c__message_type_support_callbacks_t *callbacks =
      resolve_type_support(type_support);
  if (context == NULL || topic_name == NULL || options == NULL ||
      options->rmw_specific_subscription_payload != NULL || options->ignore_local_publications ||
      options->content_filter_options != NULL ||
      options->require_unique_network_flow_endpoints !=
          RMW_UNIQUE_NETWORK_FLOW_ENDPOINTS_NOT_REQUIRED ||
      node_data == NULL || callbacks == NULL || !qos_is_supported(qos)) {
    return NULL;
  }

  rmw_subscription_t *subscription = create_subscription_handle(context, topic_name, options);
  if (subscription == NULL) {
    return NULL;
  }
  endpoint_data_t *endpoint = allocate_zeroed(&context->allocator, sizeof(*endpoint));
  char *dds_topic_name = make_dds_topic_name(&context->allocator, topic_name);
  dds_qos_t *dds_qos = create_dds_qos(qos, false, type_support);
  if (endpoint == NULL || dds_topic_name == NULL || dds_qos == NULL) {
    goto fail;
  }
  endpoint->type_support = callbacks;
  endpoint->dds_sample = allocate_zeroed(&context->allocator, callbacks->dds_size);
  atomic_flag_clear(&endpoint->sample_lock);
  if (endpoint->dds_sample == NULL) {
    goto fail;
  }
  endpoint->qos = *qos;
  endpoint->topic =
      dds_create_topic(context->participant, callbacks->descriptor, dds_topic_name, NULL, NULL);
  if (endpoint->topic < 0) {
    (void)map_dds_result(endpoint->topic, "dds_create_topic");
    goto fail;
  }
  endpoint->endpoint = dds_create_reader(context->participant, endpoint->topic, dds_qos, NULL);
  if (endpoint->endpoint < 0) {
    (void)map_dds_result(endpoint->endpoint, "dds_create_reader");
    goto fail;
  }
  endpoint->condition = dds_create_readcondition(endpoint->endpoint, DDS_ANY_STATE);
  if (endpoint->condition < 0) {
    (void)map_dds_result(endpoint->condition, "dds_create_readcondition");
    goto fail;
  }
  const dds_return_t guid_result = dds_get_guid(endpoint->endpoint, &endpoint->guid);
  if (guid_result < 0) {
    (void)map_dds_result(guid_result, "dds_get_guid reader");
    goto fail;
  }
  endpoint->node = node_data;
  if (graph_add_endpoint(context, node_data, &endpoint->guid, false) != RMW_RET_OK) {
    goto fail;
  }
  dds_delete_qos(dds_qos);
  context->allocator.deallocate(dds_topic_name, context->allocator.state);
  subscription->data = endpoint;
  (void)dds_set_guardcondition(context->graph_guard_entity, true);
  return subscription;

fail:
  if (dds_qos != NULL) {
    dds_delete_qos(dds_qos);
  }
  if (dds_topic_name != NULL) {
    context->allocator.deallocate(dds_topic_name, context->allocator.state);
  }
  if (endpoint != NULL) {
    if (endpoint->endpoint > 0) {
      (void)dds_delete(endpoint->endpoint);
    }
    if (endpoint->topic > 0) {
      (void)dds_delete(endpoint->topic);
    }
    if (endpoint->dds_sample != NULL) {
      context->allocator.deallocate(endpoint->dds_sample, context->allocator.state);
    }
    context->allocator.deallocate(endpoint, context->allocator.state);
  }
  context->allocator.deallocate((void *)subscription->topic_name, context->allocator.state);
  context->allocator.deallocate(subscription, context->allocator.state);
  return NULL;
}

rmw_ret_t rmw_destroy_subscription(rmw_node_t *node, rmw_subscription_t *subscription)
{
  rmw_context_impl_t *context = context_impl_from_node(node);
  if (context == NULL || subscription == NULL ||
      !identifiers_match(subscription->implementation_identifier) || subscription->data == NULL) {
    RMW_SET_ERROR_MSG("invalid subscription destruction arguments");
    return RMW_RET_INVALID_ARGUMENT;
  }
  endpoint_data_t *endpoint = subscription->data;
  rmw_ret_t result = graph_remove_endpoint(context, endpoint->node, &endpoint->guid, false);
  const rmw_ret_t condition_result =
      map_dds_result(dds_delete(endpoint->condition), "dds_delete condition");
  const rmw_ret_t reader_result =
      map_dds_result(dds_delete(endpoint->endpoint), "dds_delete reader");
  const rmw_ret_t topic_result = map_dds_result(dds_delete(endpoint->topic), "dds_delete topic");
  if (condition_result != RMW_RET_OK || reader_result != RMW_RET_OK ||
      topic_result != RMW_RET_OK) {
    result = RMW_RET_ERROR;
  }
  context->allocator.deallocate(endpoint->dds_sample, context->allocator.state);
  context->allocator.deallocate(endpoint, context->allocator.state);
  context->allocator.deallocate((void *)subscription->topic_name, context->allocator.state);
  context->allocator.deallocate(subscription, context->allocator.state);
  (void)dds_set_guardcondition(context->graph_guard_entity, true);
  return result;
}

rmw_ret_t rmw_subscription_get_actual_qos(const rmw_subscription_t *subscription,
                                          rmw_qos_profile_t *qos)
{
  if (subscription == NULL || qos == NULL ||
      !identifiers_match(subscription->implementation_identifier) || subscription->data == NULL) {
    RMW_SET_ERROR_MSG("invalid subscription QoS query arguments");
    return RMW_RET_INVALID_ARGUMENT;
  }
  *qos = ((const endpoint_data_t *)subscription->data)->qos;
  return RMW_RET_OK;
}

rmw_ret_t rmw_subscription_count_matched_publishers(const rmw_subscription_t *subscription,
                                                    size_t *publisher_count)
{
  if (subscription == NULL || publisher_count == NULL ||
      !identifiers_match(subscription->implementation_identifier) || subscription->data == NULL) {
    RMW_SET_ERROR_MSG("invalid subscription match-count arguments");
    return RMW_RET_INVALID_ARGUMENT;
  }
  const endpoint_data_t *endpoint = subscription->data;
  const dds_return_t count = dds_get_matched_publications(endpoint->endpoint, NULL, 0U);
  if (count < 0) {
    return map_dds_result(count, "dds_get_matched_publications");
  }
  *publisher_count = (size_t)count;
  return RMW_RET_OK;
}

rmw_ret_t rmw_take(const rmw_subscription_t *subscription, void *ros_message, bool *taken,
                   rmw_subscription_allocation_t *allocation)
{
  rmw_message_info_t message_info = rmw_get_zero_initialized_message_info();
  return rmw_take_with_info(subscription, ros_message, taken, &message_info, allocation);
}

rmw_ret_t rmw_take_with_info(const rmw_subscription_t *subscription, void *ros_message, bool *taken,
                             rmw_message_info_t *message_info,
                             rmw_subscription_allocation_t *allocation)
{
  if (subscription == NULL || ros_message == NULL || taken == NULL || message_info == NULL ||
      !identifiers_match(subscription->implementation_identifier) || subscription->data == NULL) {
    RMW_SET_ERROR_MSG("invalid take arguments");
    return RMW_RET_INVALID_ARGUMENT;
  }
  if (allocation != NULL) {
    RMW_SET_ERROR_MSG("subscription allocations are not supported");
    return RMW_RET_UNSUPPORTED;
  }

  endpoint_data_t *endpoint = subscription->data;
  lock_sample(endpoint);
  memset(endpoint->dds_sample, 0, endpoint->type_support->dds_size);
  void *samples[] = {endpoint->dds_sample};
  dds_sample_info_t sample_info;
  const dds_return_t take_result = dds_take(endpoint->endpoint, samples, &sample_info, 1U, 1U);
  if (take_result < 0) {
    unlock_sample(endpoint);
    return map_dds_result(take_result, "dds_take");
  }
  if (take_result == 0 || !sample_info.valid_data) {
    unlock_sample(endpoint);
    *taken = false;
    return RMW_RET_OK;
  }

  if (!endpoint->type_support->dds_to_ros(endpoint->dds_sample, ros_message)) {
    unlock_sample(endpoint);
    RMW_SET_ERROR_MSG("generated DDS-to-ROS conversion failed");
    return RMW_RET_ERROR;
  }
  unlock_sample(endpoint);
  *message_info = rmw_get_zero_initialized_message_info();
  message_info->source_timestamp = sample_info.source_timestamp;
  message_info->received_timestamp = dds_time();
  message_info->publication_sequence_number = RMW_MESSAGE_INFO_SEQUENCE_NUMBER_UNSUPPORTED;
  message_info->reception_sequence_number = RMW_MESSAGE_INFO_SEQUENCE_NUMBER_UNSUPPORTED;
  message_info->publisher_gid.implementation_identifier = implementation_identifier;
  const size_t handle_size = sizeof(sample_info.publication_handle) < RMW_GID_STORAGE_SIZE
                                 ? sizeof(sample_info.publication_handle)
                                 : RMW_GID_STORAGE_SIZE;
  memcpy(message_info->publisher_gid.data, &sample_info.publication_handle, handle_size);
  message_info->from_intra_process = false;
  *taken = true;
  return RMW_RET_OK;
}

rmw_ret_t rmw_trigger_guard_condition(const rmw_guard_condition_t *guard_condition)
{
  if (guard_condition == NULL || !identifiers_match(guard_condition->implementation_identifier) ||
      guard_condition->data == NULL) {
    RMW_SET_ERROR_MSG("invalid guard condition");
    return RMW_RET_INVALID_ARGUMENT;
  }
  const dds_entity_t entity = *(const dds_entity_t *)guard_condition->data;
  return map_dds_result(dds_set_guardcondition(entity, true), "dds_set_guardcondition");
}

rmw_ret_t rmw_compare_gids_equal(const rmw_gid_t *gid1, const rmw_gid_t *gid2, bool *result)
{
  if (gid1 == NULL || gid2 == NULL || result == NULL ||
      !identifiers_match(gid1->implementation_identifier) ||
      !identifiers_match(gid2->implementation_identifier)) {
    RMW_SET_ERROR_MSG("invalid GID comparison arguments");
    return RMW_RET_INVALID_ARGUMENT;
  }
  *result = memcmp(gid1->data, gid2->data, RMW_GID_STORAGE_SIZE) == 0;
  return RMW_RET_OK;
}

rmw_wait_set_t *rmw_create_wait_set(rmw_context_t *context, size_t max_conditions)
{
  if (context == NULL || !identifiers_match(context->implementation_identifier) ||
      context->impl == NULL || context->impl->shutdown) {
    RMW_SET_ERROR_MSG("invalid context for wait set");
    return NULL;
  }
  const size_t capacity = max_conditions == 0U ? 16U : max_conditions;
  if (capacity > SIZE_MAX / sizeof(dds_entity_t) || capacity > SIZE_MAX / sizeof(dds_attach_t)) {
    RMW_SET_ERROR_MSG("wait set capacity allocation overflow");
    return NULL;
  }
  rcutils_allocator_t *allocator = &context->impl->allocator;
  rmw_wait_set_t *wait_set = allocate_zeroed(allocator, sizeof(*wait_set));
  wait_set_data_t *data = allocate_zeroed(allocator, sizeof(*data));
  if (wait_set == NULL || data == NULL) {
    goto fail;
  }
  data->allocator = *allocator;
  data->capacity = capacity;
  data->attached = allocate_zeroed(allocator, capacity * sizeof(*data->attached));
  data->triggered = allocate_zeroed(allocator, capacity * sizeof(*data->triggered));
  if (data->attached == NULL || data->triggered == NULL) {
    goto fail;
  }
  data->waitset = dds_create_waitset(context->impl->participant);
  if (data->waitset < 0) {
    (void)map_dds_result(data->waitset, "dds_create_waitset");
    goto fail;
  }
  wait_set->implementation_identifier = implementation_identifier;
  wait_set->data = data;
  return wait_set;

fail:
  if (data != NULL) {
    if (data->attached != NULL) {
      allocator->deallocate(data->attached, allocator->state);
    }
    if (data->triggered != NULL) {
      allocator->deallocate(data->triggered, allocator->state);
    }
    allocator->deallocate(data, allocator->state);
  }
  if (wait_set != NULL) {
    allocator->deallocate(wait_set, allocator->state);
  }
  return NULL;
}

rmw_ret_t rmw_destroy_wait_set(rmw_wait_set_t *wait_set)
{
  if (wait_set == NULL || wait_set->implementation_identifier == NULL || wait_set->data == NULL) {
    RMW_SET_ERROR_MSG("invalid wait set");
    return RMW_RET_INVALID_ARGUMENT;
  }
  if (!identifiers_match(wait_set->implementation_identifier)) {
    RMW_SET_ERROR_MSG("incorrect RMW implementation for wait set");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }
  wait_set_data_t *data = wait_set->data;
  rcutils_allocator_t allocator = data->allocator;
  rmw_ret_t result = map_dds_result(dds_delete(data->waitset), "dds_delete waitset");
  allocator.deallocate(data->attached, allocator.state);
  allocator.deallocate(data->triggered, allocator.state);
  allocator.deallocate(data, allocator.state);
  allocator.deallocate(wait_set, allocator.state);
  return result;
}

static bool attachment_was_triggered(const wait_set_data_t *data, dds_entity_t attachment,
                                     size_t triggered_count)
{
  for (size_t index = 0U; index < triggered_count; ++index) {
    if (data->triggered[index] == (dds_attach_t)attachment) {
      return true;
    }
  }
  return false;
}

static rmw_ret_t attach_entity(wait_set_data_t *data, dds_entity_t entity)
{
  if (data->attached_count >= data->capacity) {
    RMW_SET_ERROR_MSG("wait set capacity exceeded");
    return RMW_RET_INVALID_ARGUMENT;
  }
  const dds_return_t result = dds_waitset_attach(data->waitset, entity, (dds_attach_t)entity);
  if (result < 0) {
    return map_dds_result(result, "dds_waitset_attach");
  }
  data->attached[data->attached_count++] = entity;
  return RMW_RET_OK;
}

rmw_ret_t rmw_wait(rmw_subscriptions_t *subscriptions, rmw_guard_conditions_t *guard_conditions,
                   rmw_services_t *services, rmw_clients_t *clients, rmw_events_t *events,
                   rmw_wait_set_t *wait_set, const rmw_time_t *wait_timeout)
{
  if (wait_set == NULL || !identifiers_match(wait_set->implementation_identifier) ||
      wait_set->data == NULL) {
    RMW_SET_ERROR_MSG("invalid wait set");
    return RMW_RET_INVALID_ARGUMENT;
  }
  if ((services != NULL && services->service_count != 0U) ||
      (clients != NULL && clients->client_count != 0U) ||
      (events != NULL && events->event_count != 0U)) {
    RMW_SET_ERROR_MSG("services, clients, and events are outside the supported profile");
    return RMW_RET_UNSUPPORTED;
  }

  wait_set_data_t *data = wait_set->data;
  for (size_t index = 0U; index < data->attached_count; ++index) {
    const dds_return_t detach_result = dds_waitset_detach(data->waitset, data->attached[index]);
    if (detach_result < 0) {
      return map_dds_result(detach_result, "dds_waitset_detach");
    }
  }
  data->attached_count = 0U;

  if (subscriptions != NULL) {
    for (size_t index = 0U; index < subscriptions->subscriber_count; ++index) {
      if (subscriptions->subscribers[index] == NULL) {
        RMW_SET_ERROR_MSG("null subscription in wait set");
        return RMW_RET_INVALID_ARGUMENT;
      }
      endpoint_data_t *endpoint = subscriptions->subscribers[index];
      const rmw_ret_t result = attach_entity(data, endpoint->condition);
      if (result != RMW_RET_OK) {
        return result;
      }
    }
  }
  if (guard_conditions != NULL) {
    for (size_t index = 0U; index < guard_conditions->guard_condition_count; ++index) {
      if (guard_conditions->guard_conditions[index] == NULL) {
        RMW_SET_ERROR_MSG("null guard condition in wait set");
        return RMW_RET_INVALID_ARGUMENT;
      }
      const dds_entity_t entity = *(dds_entity_t *)guard_conditions->guard_conditions[index];
      const rmw_ret_t result = attach_entity(data, entity);
      if (result != RMW_RET_OK) {
        return result;
      }
    }
  }

  dds_duration_t timeout = DDS_INFINITY;
  if (wait_timeout != NULL) {
    const rmw_duration_t nanoseconds = rmw_time_total_nsec(*wait_timeout);
    timeout = nanoseconds < 0 ? DDS_INFINITY : (dds_duration_t)nanoseconds;
  }
  const dds_return_t wait_result =
      dds_waitset_wait(data->waitset, data->triggered, data->capacity, timeout);
  if (wait_result < 0) {
    return map_dds_result(wait_result, "dds_waitset_wait");
  }
  const size_t triggered_count =
      (size_t)wait_result < data->capacity ? (size_t)wait_result : data->capacity;

  if (subscriptions != NULL) {
    for (size_t index = 0U; index < subscriptions->subscriber_count; ++index) {
      endpoint_data_t *endpoint = subscriptions->subscribers[index];
      if (!attachment_was_triggered(data, endpoint->condition, triggered_count)) {
        subscriptions->subscribers[index] = NULL;
      }
    }
  }
  if (guard_conditions != NULL) {
    for (size_t index = 0U; index < guard_conditions->guard_condition_count; ++index) {
      const dds_entity_t entity = *(dds_entity_t *)guard_conditions->guard_conditions[index];
      if (attachment_was_triggered(data, entity, triggered_count)) {
        bool was_triggered = false;
        (void)dds_take_guardcondition(entity, &was_triggered);
      } else {
        guard_conditions->guard_conditions[index] = NULL;
      }
    }
  }
  return wait_result == 0 ? RMW_RET_TIMEOUT : RMW_RET_OK;
}
