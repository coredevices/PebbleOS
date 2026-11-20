# NimBLE: Broadcaster

This sample brings up the NimBLE host and advertises without accepting
connections or scan requests: what a beacon does. The host runs on its own
thread, started by `nimble_port_pebble_init()`, and the sample waits for the
host to sync with the controller before it uses it.

The advertisement carries the device name and manufacturer data under
company ID `0xFFFF`, which the Bluetooth SIG reserves for testing. Its last
byte is a counter, updated every second while advertising, so a scanner such
as the {doc}`observer <observer>` sample sees it change.

On the emulator the controller is a fake one, which accepts every command: the
host runs, but nothing goes on the air.

```{pebbleos-sample}
:path: samples/third_party/nimble/broadcaster
:sources: src/main.c
:output:
```
