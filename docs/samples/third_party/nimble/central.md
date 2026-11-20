# NimBLE: Central

This sample is the other end of the {doc}`peripheral <peripheral>` sample. It scans
until it hears a device advertising that sample's service UUID, connects to
it, and works through its GATT service as a client:

- It discovers the service by UUID, then its characteristics.
- It reads the message, writes its own, and reads it back.
- It finds the CCCD of the counter among the descriptors that follow it, and
  writes it to subscribe, then prints three notifications and disconnects.

Each step starts the next from its completion callback, on the host thread;
the main thread only waits for the end. On the emulator, nothing is ever
found, so the sample keeps scanning.

```{pebbleos-sample}
:path: samples/third_party/nimble/central
:sources: src/main.c
:output:
```
