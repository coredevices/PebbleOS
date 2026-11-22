# USB: Serial port echo

This sample is a serial port over USB that sends back what the host sends,
for a minute. The receive callback of the port runs in the controller's
interrupt, so it only queues the bytes, and the main thread echoes them.

Connect the board's USB device port to a computer, which then has a new
serial port, as `/dev/cu.usbmodem*` on macOS or `/dev/ttyACM*` on Linux, and
type into it. The port has no flow control: bursts larger than the sample's
queue lose bytes.

```{pebbleos-sample}
:path: samples/subsys/usb/cdc_acm
:sources: src/main.c
:output:
```
