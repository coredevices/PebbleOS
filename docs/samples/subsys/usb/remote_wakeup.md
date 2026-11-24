# USB: Remote wakeup

This sample follows the state of the USB bus for a minute, printing each
change: connected, reset, suspended, resumed. When the host has kept the bus
suspended for 5 s, it wakes the host up with `pbl_usbd_wakeup()`. The device
advertises remote wakeup with `CONFIG_USB_DEVICE_REMOTE_WAKEUP`, and has a
serial port for the host to drive, as {doc}`cdc_acm`.

Connect the board's USB device port to a computer, then let the computer
suspend the bus, as it does when it goes to sleep. A host only allows remote
wakeup on the bus it suspends: without that, `pbl_usbd_wakeup()` returns
`-EACCES`.

```{pebbleos-sample}
:path: samples/subsys/usb/remote_wakeup
:sources: src/main.c
:output:
```
