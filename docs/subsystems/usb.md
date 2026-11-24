# USB

`subsys/usb/` is the device side of USB: the board is a USB device, a host
enumerates it. Its public API is `include/pbl/usb/usbd.h`. It is built with
`CONFIG_USB_DEVICE`, on the board's USB device controller, its `udc` role.

```{mermaid}
flowchart TD
  fn["Functions<br/>CDC ACM, DFU, ..."]
  usbd["Device stack<br/>subsys/usb/usbd.c"]
  udc["USB device controller<br/>pbl/drivers/udc.h"]
  drv["Controller driver<br/>MUSB, ..."]
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

### Strings

A function names its strings with indices of its own, from 1, in the
`iInterface` field of its interface descriptors and the `iFunction` field of
its interface association descriptor, and lists them in `strings`: index `n`
is `strings[n - 1]`. The stack numbers the strings of the device, after the
manufacturer and the product, in the order of the functions, and puts those
numbers in the configuration descriptor as it sends it, so the descriptors
stay constant and functions do not have to know about each other. Indices in
class-specific descriptors are left as they are.

### Alternate settings

An interface can have alternate settings, each an interface descriptor with
the same `bInterfaceNumber`, its own `bAlternateSetting` and the endpoint
descriptors that follow it. Settings of an interface may use the same
endpoint addresses, with other types or packet sizes. Configuring the device
selects setting 0 of every interface and enables its endpoints. On
`SET_INTERFACE`, the stack disables the endpoints of the current setting,
which ends their transfers with `-ECONNABORTED`, enables those of the new
one, and calls the function's `set_alt(iface, alt)`, where the function
starts its transfers on them. A setting the interface does not have is
stalled, and so are both requests while the device is not configured.
`GET_INTERFACE` answers with the current setting. The stack keeps the
settings of the first 16 interfaces; the others only have setting 0.

```c
static const char *const s_strings[] = {"Loopback off", "Loopback"};

PBL_USBD_FUNCTION_DEFINE(loopback) = {
  .desc = s_desc, /* iInterface 1 for setting 0, 2 for setting 1 */
  .desc_len = sizeof(s_desc),
  .strings = s_strings,
  .num_strings = PBL_ARRAY_SIZE(s_strings),
  .first_iface = 0U,
  .num_ifaces = 1U,
  .set_alt = prv_set_alt,
  .xfer_done = prv_xfer_done,
};
```

{doc}`../samples/subsys/usb/loopback` is a complete function.

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

`CONFIG_USB_CDC_ACM_NAME` (default `Serial port`) is the name the host
shows for it, its function and interface string.

Received bytes go to the UART's receive callback, from the interrupt, without
flow control: a callback that cannot keep up loses them. Bytes sent before the
host configures the device, or that do not fit while it does not read, are
dropped.

## Firmware upgrade

`CONFIG_USB_DFU` adds a Device Firmware Upgrade 1.1 interface in DFU mode
(`include/pbl/usb/dfu.h`): the host downloads an image to a flash partition
and uploads it back, with a standard tool such as `dfu-util`. The function
takes the interface after the serial port's, or interface 0 without it, and
uses endpoint 0 only.

The partition is a region the application defines, by the label of a
partition of the flash serving the `flash` role:

```c
PBL_USB_DFU_REGION_DEFINE(image, PART_DFU);
```

The region is alternate setting 0 of the interface; only one region is
supported for now. A download starts at the beginning of the partition: each
block is written once the subsectors it reaches are erased, and a block that
would go past the end of the partition fails with `errADDRESS`, before
anything is written. An upload reads the partition from its beginning and
ends with a short block at its end.

The flash is not accessed from the controller's interrupt. A downloaded block
is erased and written by a work item on the system work queue, and the
device answers `DFU_GETSTATUS` with `dfuDNBUSY` and a poll timeout from the
flash's subsector erase time until it is done. Upload blocks are read ahead
by the same work item, into one of two buffers while the host reads the
other. The device is manifestation tolerant: once a download ends it goes
back to `dfuIDLE`, ready for another one.

| Option | Default | Purpose |
| --- | --- | --- |
| `CONFIG_USB_DFU_TRANSFER_SIZE` | `2048` | Largest block, `wTransferSize`; the function keeps three |

### Flashing with dfu-util

With the device connected, `dfu-util` lists its interface, downloads an
image and uploads it back (`brew install dfu-util` on macOS, the `dfu-util`
package on Linux distributions):

```shell
dfu-util -l
dfu-util -d 1209:0001 -a 0 -D image.bin
dfu-util -d 1209:0001 -a 0 -U readback.bin -Z $(stat -f %z image.bin)
cmp image.bin readback.bin
```

`-Z` stops the upload at the size of the image; without it the whole
partition is read. On Linux, `stat -c %s` gives the size.

## Controllers

A controller driver implements `struct pbl_udc_ops` and reports bus events,
setup packets among them, and the end of transfers from its interrupt.

| Driver | Controllers |
| --- | --- |
| `drivers/udc/musb.c` | Mentor Graphics MUSB, as the full-speed USB controller of the SF32LB57, with its internal PHY; each endpoint number serves one direction at a time, with a 64-byte FIFO |
