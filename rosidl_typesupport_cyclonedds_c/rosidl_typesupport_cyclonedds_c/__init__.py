# SPDX-License-Identifier: Apache-2.0

from rosidl_pycommon import generate_files
from rosidl_parser.definition import Array
from rosidl_parser.definition import BasicType
from rosidl_parser.definition import NamespacedType


def is_supported_fixed_size_type(type_):
    if isinstance(type_, (BasicType, NamespacedType)):
        return True
    return isinstance(type_, Array) and isinstance(
        type_.value_type, (BasicType, NamespacedType)
    )


def generate_typesupport_cyclonedds_c(generator_arguments_file):
    mapping = {
        "idl__rosidl_typesupport_cyclonedds_c.h.em": "detail/%s__rosidl_typesupport_cyclonedds_c.h",
        "idl__type_support_c.c.em": "detail/%s__type_support_c.c",
    }
    return generate_files(generator_arguments_file, mapping)
