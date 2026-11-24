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
dev.write(0x04, b"hello")
print(bytes(dev.read(0x85, 512)))
```

```{pebbleos-sample}
:path: samples/subsys/usb/webusb
:sources: src/main.c
:output:
```
