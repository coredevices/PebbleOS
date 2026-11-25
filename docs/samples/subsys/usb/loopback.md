# USB: Loopback with alternate settings

This sample defines a USB function of its own, a vendor-specific interface
with three alternate settings: setting 0 has no endpoints, setting 1 loops
data back on a pair of bulk endpoints, and setting 2 on the same endpoint
addresses as interrupt ones. Each setting names itself with a string of the
function, which the stack numbers for the device. When the host selects a
setting, the stack switches the endpoints and calls the function's
`set_alt()`, which starts reading; what arrives is sent back.

Connect the board's USB device port to a computer, and select a setting and
exchange data with a USB library, as pyusb:

```python
import usb.core

dev = usb.core.find(idVendor=0x1209, idProduct=0x0001)
dev.set_interface_altsetting(interface=0, alternate_setting=1)
ep_out, ep_in = dev.get_active_configuration()[(0, 1)]
ep_out.write(b"hello")
print(bytes(ep_in.read(64)))
```

The stack picks the endpoints of the device, so the script takes them from
the descriptors of the setting.

```{pebbleos-sample}
:path: samples/subsys/usb/loopback
:sources: src/main.c
:output:
```
