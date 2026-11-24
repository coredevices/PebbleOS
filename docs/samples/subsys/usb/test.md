# USB: Test function

This sample runs the USB test function (`CONFIG_USB_TEST`) and nothing else,
for the integration tests in `tests/integration/usb`, which drive it from the
host the board's USB device port is connected to. See {doc}`USB
<../../../subsystems/usb>` for what the function does, and
{doc}`../../../development/tests/integration` for running the tests.

```{pebbleos-sample}
:path: samples/subsys/usb/test
:sources: src/main.c
:output:
```
