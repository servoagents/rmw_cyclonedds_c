#!/usr/bin/env python3

from rosidl_parser.definition import Array
from rosidl_parser.definition import BasicType
from rosidl_parser.definition import NamespacedType
from rosidl_parser.definition import UnboundedSequence
from rosidl_parser.definition import UnboundedString

from rosidl_typesupport_cyclonedds_c import is_supported_fixed_size_type


def main():
    uint32_type = BasicType('uint32')
    nested_type = NamespacedType(('phase4_test_msgs', 'msg'), 'UInt32')
    accepted = (
        is_supported_fixed_size_type(uint32_type) and
        is_supported_fixed_size_type(Array(uint32_type, 4)) and
        is_supported_fixed_size_type(nested_type) and
        is_supported_fixed_size_type(Array(nested_type, 2)))
    rejected = (
        not is_supported_fixed_size_type(UnboundedString()) and
        not is_supported_fixed_size_type(UnboundedSequence(uint32_type)))
    if not accepted or not rejected:
        raise SystemExit('fixed-size profile validation failed')
    print('PHASE4_PROFILE_PASS string=rejected sequence=rejected')


if __name__ == '__main__':
    main()
