# Pebble protocol: USB

This sample opens Pebble protocol sessions over USB
(`pbl/pp/transports/usb.h`) and over the serial console, with a few
endpoints a host tool needs: ping, the watch's version, which libpebble2 asks
for when it connects, and app logs, which the sample sends once a second
while the host wants them. A last endpoint measures throughput: it takes
what the host sends, sends the host as many messages as it asks for, and
answers once all that came before arrived.

Connect the board's USB device port to a computer, then talk to the sample
with `pbl-watch` (`tools/libs/pbl-watch`), or over the console with
`--console` and the board's serial port:

```shell
pbl-watch ping --usb
pbl-watch logs --usb
```

As the protocol's main thread, the sample gives the subsystem a work queue of
its own, as {doc}`gatt` does.

```{pebbleos-sample}
:path: samples/subsys/pp/usb
:sources: src/main.c
:output:
```
