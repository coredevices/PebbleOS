# NimBLE: Peripheral

This sample is a connectable peripheral with a GATT service of its own, under
a 128-bit UUID, that it advertises with its name. The service has two
characteristics:

- A message of up to 20 bytes, to read and write. The sample prints it when a
  peer writes it.
- A counter the main thread increments every second, to read or subscribe
  to. While a peer
  is subscribed, the main thread calls `ble_gatts_chr_updated()` every
  second, and the host reads the new value through the characteristic's
  access callback to notify it.

The services are registered before the host starts, as NimBLE requires. The
sample prints the connections, subscriptions and MTU exchanges it sees, and
advertises again after a disconnection. Any GATT client, such as a phone app
for Bluetooth Low Energy development, can connect to it.

```{pebbleos-sample}
:path: samples/third_party/nimble/peripheral
:sources: src/main.c
:output:
```
