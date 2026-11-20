# NimBLE: Heart rate central

This sample is the client of the {doc}`heart rate peripheral <peripheral_hr>` sample, or of any
heart rate sensor. It scans for a device listing the Heart Rate service in
its advertisement, connects to it, finds the measurement characteristic and
its CCCD, and subscribes. It prints ten measurements, reading the rate as
8 or 16 bits as the flags of each say, then disconnects.

```{pebbleos-sample}
:path: samples/third_party/nimble/central_hr
:sources: src/main.c
:output:
```
