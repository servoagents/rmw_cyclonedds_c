// SPDX-License-Identifier: Apache-2.0

#ifndef ROSIDL_TYPESUPPORT_CYCLONEDDS_C__MESSAGE_TYPE_SUPPORT_H_
#define ROSIDL_TYPESUPPORT_CYCLONEDDS_C__MESSAGE_TYPE_SUPPORT_H_

#include <stdbool.h>
#include <stddef.h>

#include <dds/dds.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif // ROSIDL_TYPESUPPORT_CYCLONEDDS_C__MESSAGE_TYPE_SUPPORT_H_
