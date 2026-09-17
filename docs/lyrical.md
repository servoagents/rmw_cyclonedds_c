# ROS 2 Lyrical compatibility

The Lyrical core lane builds all packages and passes the loader ABI, smoke,
contract, generated conversion, and C-to-C best-effort and reliable tests. The
normal Linux interoperability lane does not exchange samples with the stock
`rmw_cyclonedds_cpp`.

The two implementations discover each other, but Cyclone DDS rejects the
endpoint match because their XTypes identifiers differ. The installed Lyrical
DDS IDL uses escaped member names such as `data_`; the dynamic type created by
the stock RMW from ROS introspection uses `data`. Generating the experimental
IDL without the suffix makes the identifiers equal, confirming the source of
the mismatch, but changing this repository's generator would create an
incompatible type contract and hide the distribution-level problem.

The accepted ESP32-S3 Wi-Fi test used a different matching mode.
`ros2_zephyr` builds embedded Cyclone DDS with `ENABLE_TYPELIB=OFF` and
`ENABLE_TYPE_DISCOVERY=OFF`. The generated descriptor still contains the
`data_` TypeInformation, but the embedded endpoint does not advertise or use
it for matching. Stock Lyrical therefore matched by DDS topic and type name in
that lane. The hardware result demonstrates interoperability for that embedded
configuration; it does not establish interoperability between two
TypeInformation-enabled Lyrical implementations.

Run the accepted Lyrical lane with:

```sh
RMW_CYCLONEDDS_C_ROS_DISTRO=lyrical scripts/test_core.sh
```

The complete Kilted interoperability lane remains:

```sh
RMW_CYCLONEDDS_C_ROS_DISTRO=kilted scripts/test.sh
```

Full Lyrical interoperability can be enabled after its generated DDS IDL and
dynamic type construction agree on member naming. Disabling TypeInformation
in a desktop build or setting Cyclone's vendor ignore option remains a
diagnostic, not the Linux product configuration.
