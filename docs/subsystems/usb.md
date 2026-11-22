# USB

`subsys/usb/` is the device side of USB: the board is a USB device, a host
enumerates it. Its public API is `include/pbl/usb/usbd.h`. It is built with
`CONFIG_USB_DEVICE`, on the board's USB device controller, its `udc` role.

```{mermaid}
flowchart TD
  fn["Functions<br/>CDC ACM, ..."]
  usbd["Device stack<br/>subsys/usb/usbd.c"]
  udc["USB device controller<br/>pbl/drivers/udc.h"]
  drv["Controller driver"]
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

## Serial port

`CONFIG_USB_CDC_ACM` adds a serial port, CDC ACM, as a UART:
`USB_CDC_ACM_UART` in `include/pbl/usb/cdc_acm.h`. The device connects to the
bus when the port comes up. A board or an application can give it a role, as
the console's:

```yaml
console_uart:
  symbol: USB_CDC_ACM_UART
  include: pbl/usb/cdc_acm.h
```

Received bytes go to the UART's receive callback, from the interrupt, without
flow control: a callback that cannot keep up loses them. Bytes sent before the
host configures the device, or that do not fit while it does not read, are
dropped.

## Controllers

A controller driver implements `struct pbl_udc_ops` and reports bus events,
setup packets among them, and the end of transfers from its interrupt.
