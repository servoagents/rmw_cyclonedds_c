# ROS 2 Lyrical compatibility

The Lyrical lane builds all packages and passes the loader ABI, smoke, contract,
and generated conversion tests. It does not yet run the two tests that exchange
samples with the stock `rmw_cyclonedds_cpp`.

The two implementations discover each other, but Cyclone DDS rejects the
endpoint match because their XTypes identifiers differ. The installed Lyrical
DDS IDL uses escaped member names such as `data_`; the dynamic type created by
the stock RMW from ROS introspection uses `data`. Generating the experimental
IDL without the suffix makes the identifiers equal, confirming the source of
the mismatch, but changing this repository's generator would create an
incompatible type contract and hide the distribution-level problem.

Run the accepted Lyrical lane with:

```sh
RMW_CYCLONEDDS_C_ROS_DISTRO=lyrical scripts/test_core.sh
```

The complete Kilted interoperability lane remains:

```sh
RMW_CYCLONEDDS_C_ROS_DISTRO=kilted scripts/test.sh
```

Full Lyrical interoperability can be enabled after its generated DDS IDL and
dynamic type construction agree on member naming.
