/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdatomic.h>
#include <string.h>

#include <pbl/drivers/udc.h>
#include <pbl/kernel/irq.h>
#include <pbl/usb/cdc_acm.h>
#include <pbl/usb/usbd.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/listify.h>
#include <pbl/util/math.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#define NUM_PORTS CONFIG_USB_CDC_ACM_COUNT

// The function's own interfaces, name string and endpoints of port n
#define IFACE_COMM(n)  (2U * (n))
#define IFACE_DATA(n)  (2U * (n) + 1U)
#define STRING_NAME(n) ((n) + 1U)
#define EP_NOTIFY(n)   (PBL_UDC_EP_IN | (2U * (n) + 1U))
#define EP_DATA_OUT(n) ((n) + 1U)
#define EP_DATA_IN(n)  (PBL_UDC_EP_IN | (2U * (n) + 2U))

#define CDC_CLASS            0x02U
#define CDC_SUBCLASS_ACM     0x02U
#define CDC_PROTOCOL_AT      0x01U
#define CDC_DATA_CLASS       0x0aU
#define CDC_FUNC_HEADER      0x00U
#define CDC_FUNC_CALL_MGMT   0x01U
#define CDC_FUNC_ACM         0x02U
#define CDC_FUNC_UNION       0x06U
#define CDC_ACM_CAP_LINE     0x02U
#define CDC_SET_LINE_CODING  0x20U
#define CDC_GET_LINE_CODING  0x21U
#define CDC_SET_CONTROL_LINE 0x22U
#define CDC_SEND_BREAK       0x23U
#define CDC_LINE_DTR         0x01U

// Largest bulk packet, of high speed
#define BUF_LEN 512U
#define TX_RING CONFIG_USB_CDC_ACM_TX_BUF_SIZE

struct PBL_PACKED pbl_usb_cdc__desc_header {
  uint8_t bFunctionLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint16_t bcdCDC;
};

struct PBL_PACKED pbl_usb_cdc__desc_call_mgmt {
  uint8_t bFunctionLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bmCapabilities;
  uint8_t bDataInterface;
};

struct PBL_PACKED pbl_usb_cdc__desc_acm {
  uint8_t bFunctionLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bmCapabilities;
};

struct PBL_PACKED pbl_usb_cdc__desc_union {
  uint8_t bFunctionLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bControlInterface;
  uint8_t bSubordinateInterface0;
};

struct PBL_PACKED pbl_usb_cdc__desc_port {
  struct pbl_usb_desc_iad iad;
  struct pbl_usb_desc_iface comm;
  struct pbl_usb_cdc__desc_header header;
  struct pbl_usb_cdc__desc_call_mgmt call_mgmt;
  struct pbl_usb_cdc__desc_acm acm;
  struct pbl_usb_cdc__desc_union cdc_union;
  struct pbl_usb_desc_ep notify;
  struct pbl_usb_desc_iface data;
  struct pbl_usb_desc_ep data_out;
  struct pbl_usb_desc_ep data_in;
};

static_assert(sizeof(struct pbl_usb_cdc__desc_port) == 66U);

#define PRV_PORT_DESC(n)                                                 \
  {                                                                      \
    .iad =                                                               \
        {                                                                \
          .bLength = sizeof(struct pbl_usb_desc_iad),                    \
          .bDescriptorType = PBL_USB_DESC_IAD,                           \
          .bFirstInterface = IFACE_COMM(n),                              \
          .bInterfaceCount = 2U,                                         \
          .bFunctionClass = CDC_CLASS,                                   \
          .bFunctionSubClass = CDC_SUBCLASS_ACM,                         \
          .bFunctionProtocol = CDC_PROTOCOL_AT,                          \
          .iFunction = STRING_NAME(n),                                   \
        },                                                               \
    .comm =                                                              \
        {                                                                \
          .bLength = sizeof(struct pbl_usb_desc_iface),                  \
          .bDescriptorType = PBL_USB_DESC_INTERFACE,                     \
          .bInterfaceNumber = IFACE_COMM(n),                             \
          .bNumEndpoints = 1U,                                           \
          .bInterfaceClass = CDC_CLASS,                                  \
          .bInterfaceSubClass = CDC_SUBCLASS_ACM,                        \
          .bInterfaceProtocol = CDC_PROTOCOL_AT,                         \
          .iInterface = STRING_NAME(n),                                  \
        },                                                               \
    .header =                                                            \
        {                                                                \
          .bFunctionLength = sizeof(struct pbl_usb_cdc__desc_header),    \
          .bDescriptorType = PBL_USB_DESC_CS_INTERFACE,                  \
          .bDescriptorSubtype = CDC_FUNC_HEADER,                         \
          .bcdCDC = PBL_CPU_TO_LE16(0x0110U),                            \
        },                                                               \
    .call_mgmt =                                                         \
        {                                                                \
          .bFunctionLength = sizeof(struct pbl_usb_cdc__desc_call_mgmt), \
          .bDescriptorType = PBL_USB_DESC_CS_INTERFACE,                  \
          .bDescriptorSubtype = CDC_FUNC_CALL_MGMT,                      \
          .bDataInterface = IFACE_DATA(n),                               \
        },                                                               \
    .acm =                                                               \
        {                                                                \
          .bFunctionLength = sizeof(struct pbl_usb_cdc__desc_acm),       \
          .bDescriptorType = PBL_USB_DESC_CS_INTERFACE,                  \
          .bDescriptorSubtype = CDC_FUNC_ACM,                            \
          .bmCapabilities = CDC_ACM_CAP_LINE,                            \
        },                                                               \
    .cdc_union =                                                         \
        {                                                                \
          .bFunctionLength = sizeof(struct pbl_usb_cdc__desc_union),     \
          .bDescriptorType = PBL_USB_DESC_CS_INTERFACE,                  \
          .bDescriptorSubtype = CDC_FUNC_UNION,                          \
          .bControlInterface = IFACE_COMM(n),                            \
          .bSubordinateInterface0 = IFACE_DATA(n),                       \
        },                                                               \
    .notify =                                                            \
        {                                                                \
          .bLength = sizeof(struct pbl_usb_desc_ep),                     \
          .bDescriptorType = PBL_USB_DESC_ENDPOINT,                      \
          .bEndpointAddress = EP_NOTIFY(n),                              \
          .bmAttributes = PBL_UDC_EP_INTERRUPT,                          \
          .wMaxPacketSize = PBL_CPU_TO_LE16(16U),                        \
          .bInterval = 16U,                                              \
        },                                                               \
    .data =                                                              \
        {                                                                \
          .bLength = sizeof(struct pbl_usb_desc_iface),                  \
          .bDescriptorType = PBL_USB_DESC_INTERFACE,                     \
          .bInterfaceNumber = IFACE_DATA(n),                             \
          .bNumEndpoints = 2U,                                           \
          .bInterfaceClass = CDC_DATA_CLASS,                             \
        },                                                               \
    .data_out =                                                          \
        {                                                                \
          .bLength = sizeof(struct pbl_usb_desc_ep),                     \
          .bDescriptorType = PBL_USB_DESC_ENDPOINT,                      \
          .bEndpointAddress = EP_DATA_OUT(n),                            \
          .bmAttributes = PBL_UDC_EP_BULK,                               \
          .wMaxPacketSize = PBL_CPU_TO_LE16(64U),                        \
        },                                                               \
    .data_in = {                                                         \
      .bLength = sizeof(struct pbl_usb_desc_ep),                         \
      .bDescriptorType = PBL_USB_DESC_ENDPOINT,                          \
      .bEndpointAddress = EP_DATA_IN(n),                                 \
      .bmAttributes = PBL_UDC_EP_BULK,                                   \
      .wMaxPacketSize = PBL_CPU_TO_LE16(64U),                            \
    },                                                                   \
  },

static const struct pbl_usb_cdc__desc_port s_desc[NUM_PORTS] = {PBL_LISTIFY(NUM_PORTS,
                                                                            PRV_PORT_DESC)};

// A single port has the name as given; several are told apart by their number
#if NUM_PORTS == 1
static const char *const s_strings[] = {CONFIG_USB_CDC_ACM_NAME};
#else
#define PRV_NAME(n) CONFIG_USB_CDC_ACM_NAME " " #n,
static const char *const s_strings[] = {PBL_LISTIFY(NUM_PORTS, PRV_NAME)};
#endif

PBL_USBD_FUNCTION_DECLARE(cdc_acm);

struct prv_port {
  atomic_bool configured;
  bool dtr;
  bool tx_busy;
  size_t tx_len;
  uint16_t tx_head;
  uint16_t tx_tail;
  uint8_t line_coding[7];
  uint8_t tx_ring[TX_RING];
  uint8_t tx_buf[BUF_LEN];
  uint8_t rx_buf[BUF_LEN];
};

struct pbl_usb_cdc_acm {
  struct pbl_uart uart;
  uint8_t port;
};

static struct prv_port s_ports[NUM_PORTS];

#define PRV_UART_PTR(n) &pbl_usb_cdc_acm_##n,
static const struct pbl_uart *const s_uarts[NUM_PORTS] = {PBL_LISTIFY(NUM_PORTS, PRV_UART_PTR)};

static uint8_t prv_port_of(const struct pbl_uart *uart) {
  return PBL_CONTAINER_OF(uart, const struct pbl_usb_cdc_acm, uart)->port;
}

static uint8_t prv_ep(uint8_t ep) {
  return pbl_usbd_ep(PBL_USBD_FUNCTION(cdc_acm), ep);
}

static uint16_t prv_ring_used(const struct prv_port *p) {
  return (uint16_t)((p->tx_head + TX_RING - p->tx_tail) % TX_RING);
}

static bool prv_open(const struct prv_port *p) {
  return atomic_load(&p->configured) && p->dtr;
}

// With the IRQ lock held: what the ring holds goes out, in one transfer of up to a buffer. The
// bytes leave the ring only once the transfer is queued, so a failed queue keeps them for later.
static void prv_tx_start(uint8_t n) {
  struct prv_port *p = &s_ports[n];
  size_t len = PBL_MIN((size_t)prv_ring_used(p), sizeof(p->tx_buf));

  for (size_t i = 0U; i < len; i++) {
    p->tx_buf[i] = p->tx_ring[(p->tx_tail + i) % TX_RING];
  }

  p->tx_busy = pbl_udc_ep_queue(UDC, prv_ep(EP_DATA_IN(n)), p->tx_buf, len) == 0;
  if (!p->tx_busy) {
    return;
  }

  p->tx_len = len;
  p->tx_tail = (uint16_t)((p->tx_tail + len) % TX_RING);
}

static void prv_rx_start(uint8_t n) {
  (void)pbl_udc_ep_queue(UDC, prv_ep(EP_DATA_OUT(n)), s_ports[n].rx_buf, pbl_usbd_bulk_mps());
}

static void prv_configured(bool on) {
  for (uint8_t n = 0U; n < NUM_PORTS; n++) {
    struct prv_port *p = &s_ports[n];

    atomic_store(&p->configured, on);
    p->dtr = false;
    p->tx_busy = false;
    if (on) {
      prv_rx_start(n);
    }
  }
}

static int prv_class_request(const struct pbl_usb_setup *setup, uint8_t **data, uint16_t *len) {
  uint8_t n = (uint8_t)setup->wIndex / 2U;
  struct prv_port *p = &s_ports[n];

  if ((setup->bmRequestType & PBL_USB_TYPE_MASK) != PBL_USB_TYPE_CLASS || n >= NUM_PORTS) {
    return -ENOTSUP;
  }

  switch (setup->bRequest) {
    case CDC_SET_LINE_CODING:
    case CDC_GET_LINE_CODING:
      *data = p->line_coding;
      *len = sizeof(p->line_coding);
      return 0;
    case CDC_SET_CONTROL_LINE:
      p->dtr = (setup->wValue & CDC_LINE_DTR) != 0U;
      if (prv_open(p) && !p->tx_busy && prv_ring_used(p) != 0U) {
        prv_tx_start(n);
      }
      return 0;
    case CDC_SEND_BREAK:
      return 0;
    default:
      return -ENOTSUP;
  }
}

// The interfaces the call management and union descriptors name
static void prv_desc_fixup(struct pbl_usb_desc_header *desc, uint8_t first_iface) {
  const struct pbl_usb_cdc__desc_header *cdc = (const struct pbl_usb_cdc__desc_header *)desc;

  if (desc->bDescriptorType != PBL_USB_DESC_CS_INTERFACE) {
    return;
  }

  if (cdc->bDescriptorSubtype == CDC_FUNC_CALL_MGMT) {
    ((struct pbl_usb_cdc__desc_call_mgmt *)desc)->bDataInterface += first_iface;
  } else if (cdc->bDescriptorSubtype == CDC_FUNC_UNION) {
    struct pbl_usb_cdc__desc_union *u = (struct pbl_usb_cdc__desc_union *)desc;

    u->bControlInterface += first_iface;
    u->bSubordinateInterface0 += first_iface;
  }
}

// The port of one of the function's endpoints
static uint8_t prv_port_of_ep(uint8_t ep) {
  uint8_t num = ep & ~PBL_UDC_EP_IN;

  return (ep & PBL_UDC_EP_IN) != 0U ? (uint8_t)((num - 1U) / 2U) : (uint8_t)(num - 1U);
}

// A transfer that fills its packets ends with a zero-length one, for the host to take it now
static void prv_xfer_done(uint8_t ep, int ret) {
  uint8_t n = prv_port_of_ep(ep);
  struct prv_port *p;

  if (n >= NUM_PORTS) {
    return;
  }

  p = &s_ports[n];
  if (ep == EP_DATA_OUT(n)) {
    for (int i = 0; i < ret; i++) {
      (void)pbl_uart_receive(s_uarts[n], p->rx_buf[i]);
    }
    if (ret >= 0 && atomic_load(&p->configured)) {
      prv_rx_start(n);
    }
    return;
  }

  if (ep != EP_DATA_IN(n)) {
    return;
  }

  p->tx_busy = false;
  if (ret < 0 || !prv_open(p)) {
    return;
  }

  if (prv_ring_used(p) != 0U) {
    prv_tx_start(n);
  } else if (p->tx_len != 0U && (p->tx_len % pbl_usbd_bulk_mps()) == 0U) {
    p->tx_len = 0U;
    p->tx_busy = pbl_udc_ep_queue(UDC, prv_ep(EP_DATA_IN(n)), nullptr, 0U) == 0;
  }
}

PBL_USBD_FUNCTION_DEFINE(cdc_acm) = {
  .desc = s_desc,
  .desc_len = sizeof(s_desc),
  .strings = s_strings,
  .num_strings = PBL_ARRAY_SIZE(s_strings),
  .desc_fixup = prv_desc_fixup,
  .configured = prv_configured,
  .class_request = prv_class_request,
  .xfer_done = prv_xfer_done,
};

// The ring is ready from the start: an early console fills it until the host opens the port
static int prv_early_init(const struct pbl_uart *uart) {
  return 0;
}

static int prv_init(const struct pbl_uart *uart) {
  // 115200 baud, 8N1, until the host sets its own
  static const uint8_t line_coding[] = {0x00, 0xc2, 0x01, 0x00, 0, 0, 8};

  memcpy(s_ports[prv_port_of(uart)].line_coding, line_coding, sizeof(line_coding));

  return pbl_usbd_enable();
}

static int prv_configure(const struct pbl_uart *uart, const struct pbl_uart_config *cfg) {
  return 0;
}

// The UART class sends with interrupts locked, so the controller cannot empty the ring meanwhile:
// a byte that does not fit is dropped. Until the host opens the port, the ring keeps what fits.
static void prv_poll_out(const struct pbl_uart *uart, uint8_t c) {
  uint8_t n = prv_port_of(uart);
  struct prv_port *p = &s_ports[n];

  pbl_irq_lock();
  if (prv_ring_used(p) < TX_RING - 1U) {
    p->tx_ring[p->tx_head] = c;
    p->tx_head = (uint16_t)((p->tx_head + 1U) % TX_RING);
    if (prv_open(p) && !p->tx_busy) {
      prv_tx_start(n);
    }
  }
  pbl_irq_unlock();
}

static int prv_rx_enable(const struct pbl_uart *uart) {
  return 0;
}

static void prv_rx_disable(const struct pbl_uart *uart) {
}

static const struct pbl_uart_ops s_ops = {
  .init = prv_init,
  .early_init = prv_early_init,
  .configure = prv_configure,
  .poll_out = prv_poll_out,
  .rx_enable = prv_rx_enable,
  .rx_disable = prv_rx_disable,
};

bool pbl_usb_cdc_acm_dtr(const struct pbl_uart *uart) {
  const struct prv_port *p = &s_ports[prv_port_of(uart)];
  bool open;

  pbl_irq_lock();
  open = prv_open(p);
  pbl_irq_unlock();

  return open;
}

#define PRV_PORT_DEFINE(n)                                                                 \
  PBL_UART_STATE_DEFINE(pbl_usb_cdc_acm_##n);                                              \
  PBL_DEVICE_INSTANCE(pbl_usb_cdc_acm_##n, struct pbl_usb_cdc_acm, struct pbl_uart, uart){ \
    .uart = PBL_UART_INIT(pbl_usb_cdc_acm_##n, "usb_acm" #n, nullptr, &s_ops, 115200),     \
    .port = (n),                                                                           \
  };

PBL_LISTIFY(NUM_PORTS, PRV_PORT_DEFINE)
