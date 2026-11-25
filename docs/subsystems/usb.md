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
`PBL_USBD_FUNCTION_DEFINE()`, in the order of their names. Each gives its
interface and endpoint descriptors, numbered on its own (see
[](#usb-numbers)). The stack answers the standard
requests on endpoint 0, builds the configuration descriptor from the
functions, setting the maximum packet size of bulk endpoints and the interval
of interrupt ones for the bus speed, and enables the endpoints when the host
configures the device. Class and vendor requests to an interface go to its
function, and so do the ends of the transfers on its endpoints. All of it runs
from the controller's interrupt.

| Option | Default | Purpose |
| --- | --- | --- |
| `CONFIG_USB_DEVICE_VID` | `0x1209` | Vendor ID, pid.codes' |
| `CONFIG_USB_DEVICE_PID` | `0x0001` | Product ID, pid.codes' test one |
| `CONFIG_USB_DEVICE_MANUFACTURER` | `PebbleOS` | Manufacturer string |
| `CONFIG_USB_DEVICE_PRODUCT` | `PebbleOS device` | Product string |

### Descriptors

`include/pbl/usb/usb.h` describes the standard descriptors as packed
structures, `struct pbl_usb_desc_iface`, `struct pbl_usb_desc_ep` and the
others, with the field names of the specification. A function gives its
descriptors as one constant structure of them, in the order they are sent,
class-specific ones defined next to the function. Their 16-bit fields are
little-endian, which `PBL_CPU_TO_LE16()` gives in a static initializer:

```c
static const struct PBL_PACKED {
  struct pbl_usb_desc_iface iface;
  struct pbl_usb_desc_ep out;
  struct pbl_usb_desc_ep in;
} s_desc = {
  .iface = {
    .bLength = sizeof(struct pbl_usb_desc_iface),
    .bDescriptorType = PBL_USB_DESC_INTERFACE,
    .bInterfaceNumber = 0U,
    .bNumEndpoints = 2U,
    .bInterfaceClass = 0xffU,
  },
  .out = {
    .bLength = sizeof(struct pbl_usb_desc_ep),
    .bDescriptorType = PBL_USB_DESC_ENDPOINT,
    .bEndpointAddress = 0x01U,
    .bmAttributes = PBL_UDC_EP_BULK,
    .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
  },
  .in = {
    .bLength = sizeof(struct pbl_usb_desc_ep),
    .bDescriptorType = PBL_USB_DESC_ENDPOINT,
    .bEndpointAddress = 0x81U,
    .bmAttributes = PBL_UDC_EP_BULK,
    .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
  },
};

PBL_USBD_FUNCTION_DEFINE(vendor) = {
  .desc = &s_desc,
  .desc_len = sizeof(s_desc),
};
```

The descriptors stay constant: the stack fixes up the copy it sends, as the
interface numbers and endpoint addresses of the device and the packet size of
bulk endpoints for the bus speed.

(usb-numbers)=

### Interface and endpoint numbers

A function numbers its interfaces from 0 and its endpoints from 1 in each
direction, as if it were the only function of the device: a function with one
bulk endpoint in each direction has `0x01` and `0x81`. When the device
connects, the stack walks the functions in order and gives each the next
interface numbers of the device and, for each of its endpoints, the first
endpoint of the controller that is free in that direction. On a controller
whose endpoint numbers each serve one direction (`PBL_UDC_CAP_EP_ONE_DIR`, as
the MUSB), an endpoint number taken in one direction is not given in the
other. The same endpoint in several alternate settings of an interface keeps
one endpoint of the controller. When the functions need more endpoints than
the controller has, more than 16 interfaces or more than 8 functions,
`pbl_usbd_enable()` fails with `-ENOSPC` and the device does not connect.

The stack puts the numbers of the device in the configuration descriptor as
it sends it: `bFirstInterface` of interface association descriptors,
`bInterfaceNumber` of interface descriptors and `bEndpointAddress` of
endpoint descriptors, and in the function subsets of the Microsoft OS 2.0
descriptor set. A class-specific descriptor that names interfaces is the
function's to patch, in its `desc_fixup(desc, first_iface)`, called on the
copy of each descriptor the stack does not know, as the serial port does for
its union and call management descriptors.

Everything the stack hands a function uses the function's own numbers: the
interface of `set_alt()`, the low byte of `wIndex` in `class_request()` and
`control_out()`, and the endpoint of `xfer_done()`. To start a transfer, a
function asks for the endpoint of the controller with `pbl_usbd_ep()`:

```c
#define EP_OUT 0x01U

PBL_USBD_FUNCTION_DECLARE(vendor);

static void prv_rx_start(void) {
  (void)pbl_udc_ep_queue(UDC, pbl_usbd_ep(PBL_USBD_FUNCTION(vendor), EP_OUT), s_buf,
                         sizeof(s_buf));
}
```

`PBL_USBD_FUNCTION_DECLARE()` lets code refer to the function before
`PBL_USBD_FUNCTION_DEFINE()`. The host reads the numbers of the device from
the descriptors; tools that select an endpoint should look it up there
rather than assume one.

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
`GET_INTERFACE` answers with the current setting.

```c
static const char *const s_strings[] = {"Loopback off", "Loopback"};

PBL_USBD_FUNCTION_DEFINE(loopback) = {
  .desc = &s_desc, /* iInterface 1 for setting 0, 2 for setting 1 */
  .desc_len = sizeof(s_desc),
  .strings = s_strings,
  .num_strings = PBL_ARRAY_SIZE(s_strings),
  .set_alt = prv_set_alt,
  .xfer_done = prv_xfer_done,
};
```

{doc}`../samples/subsys/usb/loopback` is a complete function.

## BOS, WinUSB and WebUSB

`CONFIG_USB_DEVICE_MS_OS_20` and `CONFIG_USB_DEVICE_WEBUSB` make the device
USB 2.1 (`bcdUSB` 0x0210), with a BOS descriptor: a USB 2.0 extension without
link power management, and the platform capability of each. The stack answers
their vendor requests to the device itself.

A function that gives a device interface GUID in `winusb_guid` is bound to
WinUSB on Windows hosts, without a driver package, and applications find it by
that GUID:

```c
PBL_USBD_FUNCTION_DEFINE(vendor) = {
  ...
  .winusb_guid = "{DE07B10A-24B5-440C-A638-F3855BEB7979}",
};
```

With `CONFIG_USB_DEVICE_MS_OS_20`, the stack builds the Microsoft OS 2.0
descriptor set from those functions: the WINUSB compatible ID and a
`DeviceInterfaceGUIDs` registry property for each. When the device has more
than one interface, each of those functions goes in a function subset, from its
first interface, within a subset of the configuration. The set is sent for
vendor code 0x01, and the BOS platform capability leaves it out when no
function gives a GUID.

`CONFIG_USB_DEVICE_WEBUSB` adds the WebUSB platform capability, vendor code
0x02. `CONFIG_USB_DEVICE_WEBUSB_LANDING_PAGE` is the URL browsers may offer to
open when the device connects, sent for `GET_URL`; empty, the device has none.
Browsers reach interfaces that no driver of the host has claimed, as a vendor
interface bound to WinUSB.

`samples/subsys/usb/webusb` is a vendor interface that echoes what the host
sends, with both.

## Serial ports

`CONFIG_USB_CDC_ACM` adds serial ports, CDC ACM, as UARTs:
`USB_CDC_ACM_UART(n)` in `include/pbl/usb/cdc_acm.h`, for the
`CONFIG_USB_CDC_ACM_COUNT` ports, 1 by default. The device connects to the bus
when a port comes up. A board or an application can give a port a role, as
the console's:

```yaml
console_uart:
  symbol: USB_CDC_ACM_UART(0)
  include: pbl/usb/cdc_acm.h
```

`CONFIG_USB_CDC_ACM_NAME` (default `Serial port`) is the name the host
shows for a port, its function and interface string. With several ports,
port `n` is named after it, followed by `n`: `Serial port 0`,
`Serial port 1`.

The ports are one function of the device. Port `n` is an interface
association of the function's interfaces `2n` and `2n+1`, with its endpoints
`0x81 + 2n` (notifications), `0x01 + n` (OUT) and `0x82 + 2n` (IN); the stack
numbers them, as it does for any function (see [](#usb-numbers)), so the
functions after the serial ports take the interfaces after theirs. On the
MUSB of the SF32LB57, whose seven endpoints besides endpoint 0 each serve
one direction, two ports take six of them; with more ports than the
controller has endpoints for, `pbl_usbd_enable()` fails with `-ENOSPC`.

A port sends only while the host has it open: the device configured, and DTR
held by the host, as a terminal does while it has the port open
(`pbl_usb_cdc_acm_dtr()`). Until then, the port keeps what is written, an
early console's boot output included, in a buffer of
`CONFIG_USB_CDC_ACM_TX_BUF_SIZE` bytes, 1024 by default, and sends it once
the host opens the port. Bytes that do not fit in the buffer are dropped,
whether the port is open or not: the UART's writes run with interrupts
locked, so they cannot wait for the controller to send.

The line coding the host sets is kept and read back as set; it does not
change the port. Breaks the host sends are ignored.

Received bytes go to the UART's receive callback, from the interrupt, without
flow control: a callback that cannot keep up loses them.

## Firmware upgrade

`CONFIG_USB_DFU` adds a Device Firmware Upgrade 1.1 interface in DFU mode
(`include/pbl/usb/dfu.h`): the host downloads an image to a flash partition
and uploads it back, with a standard tool such as `dfu-util`. The function
has one interface, numbered among the other functions', and uses endpoint 0
only.

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

## Test function

`CONFIG_USB_TEST` adds a vendor-class function for testing the stack and the
controller from a host, which the integration tests in `tests/integration/usb`
drive (see [](../development/tests/integration.md)), with the
`samples/subsys/usb/test` sample. It has one interface and five endpoints,
whose own addresses below are also those of the device when it is the only
function on a controller whose endpoint numbers each serve one direction:

| Endpoint | Type | What it does |
| --- | --- | --- |
| `0x01`, `0x82` | Bulk | Loopback: what arrives on `0x01` goes back on `0x82`, ending with a zero-length packet when it fills its packets |
| `0x03` | Bulk | Sink: checks that the bytes of each packet count up from its sequence number |
| `0x84` | Bulk | Source: sends packets whose bytes count up from their sequence number |
| `0x85` | Interrupt | A 16-byte packet every frame the host polls, its first four bytes its sequence number |

Its vendor requests, to its interface:

| Request | Direction | What it does |
| --- | --- | --- |
| `0x01` | OUT | Stores up to 1024 bytes; more stall |
| `0x02` | IN | Returns `wValue` bytes of what was stored |
| `0x03` | Either | Stalls |
| `0x04` | OUT | Clears the counters and the sink's sequence number |
| `0x05` | IN | Returns the counters, five 32-bit little-endian words: loopback, source and sink bytes, sink errors, interrupt packets |

## Controllers

A controller driver implements `struct pbl_udc_ops` and reports bus events,
setup packets among them, and the end of transfers from its interrupt.

| Driver | Controllers |
| --- | --- |
| `drivers/udc/musb.c` | Mentor Graphics MUSB, as the full-speed USB controller of the SF32LB57, with its internal PHY; each endpoint number serves one direction at a time (`PBL_UDC_CAP_EP_ONE_DIR`), with a FIFO its packet size rounded up to a power of two, given out as endpoints are enabled from the FIFO RAM the devicetree gives (`sifli,fifo-size`); a bulk OUT endpoint gets room for two packets while 128 bytes are left for the endpoints after it, so the host can send the next packet while the CPU reads one |
