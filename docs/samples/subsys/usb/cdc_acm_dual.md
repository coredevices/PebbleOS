# USB: Two serial ports

This sample has two serial ports over USB: the console on the first, given to
it by the sample's `roles.yml`, and an echo of what the host sends on the
second, for two minutes. Every 10 s the console tells whether the second port
is open.

Connect the board's USB device port to a computer, which then has two new
serial ports, as `/dev/cu.usbmodem*` on macOS or `/dev/ttyACM*` on Linux. The
console keeps what it prints until a terminal opens the first port, the boot
banner included, and then sends it.

```{pebbleos-sample}
:path: samples/subsys/usb/cdc_acm_dual
:sources: roles.yml src/main.c
:output:
```
