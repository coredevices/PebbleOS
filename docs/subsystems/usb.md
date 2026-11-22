# USB

`subsys/usb/` is the device side of USB: the board is a USB device, a host
enumerates it. Its public API is `include/pbl/usb/usbd.h`. It is built with
`CONFIG_USB_DEVICE`, on the board's USB device controller, its `udc` role.

```{mermaid}
flowchart TD
  fn["Functions"]
  usbd["Device stack<br/>subsys/usb/usbd.c"]
  udc["USB device controller<br/>pbl/drivers/udc.h"]
  drv["Controller driver<br/>DWC2, ..."]
  fn -- "descriptors, class requests, transfers" --> usbd
  usbd -- "endpoints, transfers" --> udc
  udc --> drv
```

## The device

The device has one configuration, made of the functions defined with
`PBL_USBD_FUNCTION_DEFINE()`. Each gives its interface and endpoint
descriptors with its own interface numbers and endpoint addresses, so the
functions of a device take different ones. The stack answers the standard
requests on endpoint 0, builds the configuration descriptor from the
functions, setting the maximum packet size of bulk endpoints and the interval
of interrupt ones for the bus speed, and enables the endpoints when the host
configures the device. Class requests to an interface go to its function, and
so do the ends of the transfers on its endpoints. All of it runs from the
controller's interrupt.

| Option | Default | Purpose |
| --- | --- | --- |
| `CONFIG_USB_DEVICE_VID` | `0x1209` | Vendor ID, pid.codes' |
| `CONFIG_USB_DEVICE_PID` | `0x0001` | Product ID, pid.codes' test one |
| `CONFIG_USB_DEVICE_MANUFACTURER` | `PebbleOS` | Manufacturer string |
| `CONFIG_USB_DEVICE_PRODUCT` | `PebbleOS device` | Product string |

## Controllers

A controller driver implements `struct pbl_udc_ops` and reports bus events,
setup packets among them, and the end of transfers from its interrupt.

| Driver | Controllers |
| --- | --- |
| `drivers/udc/dwc2.c` | Synopsys DesignWare USB 2.0 OTG, as the USB OTG HS of the STM32H7, with an external ULPI PHY or its internal full-speed PHY |
