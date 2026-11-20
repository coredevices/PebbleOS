# NimBLE: Observer

This sample scans for five seconds and lists every device it hears
advertising once, with its address, signal strength and name, when the
advertisement has one. The scan is passive: it never sends a scan request,
so it only sees what devices broadcast on their own.

The scan events come on the host thread; the sample waits on a semaphore for
the one that ends it. On the emulator, the fake controller never reports a
device.

```{pebbleos-sample}
:path: samples/third_party/nimble/observer
:sources: src/main.c
:output:
```
