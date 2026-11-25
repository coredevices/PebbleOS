# USB: Vendor interface echo

This sample is a vendor interface over USB, with a bulk endpoint in each
direction, that sends back each transfer the host sends, for a minute. A
transfer ends with a short packet or at 512 bytes.

The interface gives a device interface GUID, so Windows binds it to WinUSB
from the Microsoft OS 2.0 descriptors, and the device has the WebUSB platform
capability, with a landing page. Other hosts reach it through libusb, as with
pyusb:

```python
import usb.core

dev = usb.core.find(idVendor=0x1209, idProduct=0x0001)
ep_out, ep_in = dev.get_active_configuration()[(0, 0)]
ep_out.write(b"hello")
print(bytes(ep_in.read(512)))
```

The stack picks the endpoints of the device, so the script takes them from
the descriptors of the interface.

```{pebbleos-sample}
:path: samples/subsys/usb/webusb
:sources: src/main.c
:output:
```
