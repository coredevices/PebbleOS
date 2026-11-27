# PHY

A PHY is the transceiver of a controller, as the USB PHY of a USB
controller. The controller takes it from its devicetree `phys` property and
powers it on before using the bus. A USB PHY that senses VBUS also reports
it coming and going, which a USB device controller follows.

| | |
| --- | --- |
| Directory | `drivers/phy` |
| Header | `pbl/drivers/phy.h` |
| Kconfig | `CONFIG_PHY`, selected by the controllers that take a PHY |
| API | {apidoc}`API reference <drivers_phy>` |

## Drivers

| Driver | Compatible | Kconfig |
| --- | --- | --- |
| Nordic nRF54LM20A USBHS PHY | `nordic,nrf54lm20a-usbhs-phy` | `CONFIG_PHY_NRF54L_USBHS` |
| USB transceiver with nothing to set up | `usb-nop-xceiv` | `CONFIG_PHY_USB_NOP` |
