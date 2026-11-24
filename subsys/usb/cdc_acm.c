/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdatomic.h>
#include <string.h>

#include <pbl/drivers/udc.h>
#include <pbl/kernel/irq.h>
#include <pbl/usb/cdc_acm.h>
#include <pbl/usb/usbd.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#define IFACE_COMM 0U
#define IFACE_DATA 1U

#define STRING_NAME 1U

#define EP_NOTIFY   (PBL_UDC_EP_IN | 1U)
#define EP_DATA_OUT 2U
#define EP_DATA_IN  (PBL_UDC_EP_IN | 3U)

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

// Largest bulk packet, of high speed
#define BUF_LEN 512U
#define TX_RING 1024U

static const uint8_t s_desc[] = {
  // Association of the two interfaces
  8,
  PBL_USB_DESC_IAD,
  IFACE_COMM,
  2,
  CDC_CLASS,
  CDC_SUBCLASS_ACM,
  CDC_PROTOCOL_AT,
  STRING_NAME,
  // Communication interface, with its functional descriptors and notification endpoint
  9,
  PBL_USB_DESC_INTERFACE,
  IFACE_COMM,
  0,
  1,
  CDC_CLASS,
  CDC_SUBCLASS_ACM,
  CDC_PROTOCOL_AT,
  STRING_NAME,
  5,
  PBL_USB_DESC_CS_INTERFACE,
  CDC_FUNC_HEADER,
  0x10,
  0x01,
  5,
  PBL_USB_DESC_CS_INTERFACE,
  CDC_FUNC_CALL_MGMT,
  0,
  IFACE_DATA,
  4,
  PBL_USB_DESC_CS_INTERFACE,
  CDC_FUNC_ACM,
  CDC_ACM_CAP_LINE,
  5,
  PBL_USB_DESC_CS_INTERFACE,
  CDC_FUNC_UNION,
  IFACE_COMM,
  IFACE_DATA,
  7,
  PBL_USB_DESC_ENDPOINT,
  EP_NOTIFY,
  PBL_UDC_EP_INTERRUPT,
  16,
  0,
  16,
  // Data interface
  9,
  PBL_USB_DESC_INTERFACE,
  IFACE_DATA,
  0,
  2,
  CDC_DATA_CLASS,
  0,
  0,
  0,
  7,
  PBL_USB_DESC_ENDPOINT,
  EP_DATA_OUT,
  PBL_UDC_EP_BULK,
  64,
  0,
  0,
  7,
  PBL_USB_DESC_ENDPOINT,
  EP_DATA_IN,
  PBL_UDC_EP_BULK,
  64,
  0,
  0,
};

static const char *const s_strings[] = {
  CONFIG_USB_CDC_ACM_NAME,
};

static struct {
  atomic_bool configured;
  bool tx_busy;
  size_t tx_len;
  uint16_t tx_head;
  uint16_t tx_tail;
  // 115200 baud, 8N1, until the host sets its own
  uint8_t line_coding[7];
  uint8_t tx_ring[TX_RING];
  uint8_t tx_buf[BUF_LEN];
  uint8_t rx_buf[BUF_LEN];
} s_acm = {
  .line_coding = {0x00, 0xc2, 0x01, 0x00, 0, 0, 8},
};

static uint16_t prv_ring_used(void) {
  return (uint16_t)((s_acm.tx_head - s_acm.tx_tail) % TX_RING);
}

// With the IRQ lock held: what the ring holds goes out, in one transfer of up to a buffer
static void prv_tx_start(void) {
  size_t len = PBL_MIN((size_t)prv_ring_used(), sizeof(s_acm.tx_buf));

  for (size_t i = 0U; i < len; i++) {
    s_acm.tx_buf[i] = s_acm.tx_ring[s_acm.tx_tail];
    s_acm.tx_tail = (uint16_t)((s_acm.tx_tail + 1U) % TX_RING);
  }

  s_acm.tx_len = len;
  s_acm.tx_busy = pbl_udc_ep_queue(UDC, EP_DATA_IN, s_acm.tx_buf, len) == 0;
}

static void prv_rx_start(void) {
  (void)pbl_udc_ep_queue(UDC, EP_DATA_OUT, s_acm.rx_buf, pbl_usbd_bulk_mps());
}

static void prv_configured(bool on) {
  atomic_store(&s_acm.configured, on);
  s_acm.tx_busy = false;
  if (on) {
    prv_rx_start();
    if (prv_ring_used() != 0U) {
      prv_tx_start();
    }
  }
}

static int prv_class_request(const struct pbl_usb_setup *setup, uint8_t **data, uint16_t *len) {
  if ((setup->bmRequestType & PBL_USB_TYPE_MASK) != PBL_USB_TYPE_CLASS) {
    return -ENOTSUP;
  }

  switch (setup->bRequest) {
    case CDC_SET_LINE_CODING:
    case CDC_GET_LINE_CODING:
      *data = s_acm.line_coding;
      *len = sizeof(s_acm.line_coding);
      return 0;
    case CDC_SET_CONTROL_LINE:
    case CDC_SEND_BREAK:
      return 0;
    default:
      return -ENOTSUP;
  }
}

// A transfer that fills its packets ends with a zero-length one, for the host to take it now
static void prv_xfer_done(uint8_t ep, int ret) {
  if (ep == EP_DATA_OUT) {
    for (int i = 0; i < ret; i++) {
      (void)pbl_uart_receive(&pbl_usb_cdc_acm, s_acm.rx_buf[i]);
    }
    if (ret >= 0 && atomic_load(&s_acm.configured)) {
      prv_rx_start();
    }
    return;
  }

  if (ep != EP_DATA_IN) {
    return;
  }

  s_acm.tx_busy = false;
  if (ret < 0 || !atomic_load(&s_acm.configured)) {
    return;
  }

  if (prv_ring_used() != 0U) {
    prv_tx_start();
  } else if (s_acm.tx_len != 0U && (s_acm.tx_len % pbl_usbd_bulk_mps()) == 0U) {
    s_acm.tx_len = 0U;
    s_acm.tx_busy = pbl_udc_ep_queue(UDC, EP_DATA_IN, nullptr, 0U) == 0;
  }
}

PBL_USBD_FUNCTION_DEFINE(cdc_acm) = {
  .desc = s_desc,
  .desc_len = sizeof(s_desc),
  .strings = s_strings,
  .num_strings = PBL_ARRAY_SIZE(s_strings),
  .first_iface = IFACE_COMM,
  .num_ifaces = 2U,
  .configured = prv_configured,
  .class_request = prv_class_request,
  .xfer_done = prv_xfer_done,
};

static int prv_init(const struct pbl_uart *uart) {
  return pbl_usbd_enable();
}

static int prv_configure(const struct pbl_uart *uart, const struct pbl_uart_config *cfg) {
  return 0;
}

// The UART class sends with interrupts locked, so the controller cannot empty the ring meanwhile:
// a byte that does not fit is dropped
static void prv_poll_out(const struct pbl_uart *uart, uint8_t c) {
  pbl_irq_lock();
  if (atomic_load(&s_acm.configured) && prv_ring_used() < TX_RING - 1U) {
    s_acm.tx_ring[s_acm.tx_head] = c;
    s_acm.tx_head = (uint16_t)((s_acm.tx_head + 1U) % TX_RING);
    if (!s_acm.tx_busy) {
      prv_tx_start();
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
  .configure = prv_configure,
  .poll_out = prv_poll_out,
  .rx_enable = prv_rx_enable,
  .rx_disable = prv_rx_disable,
};

struct pbl_usb_cdc_acm {
  struct pbl_uart uart;
};

PBL_UART_STATE_DEFINE(pbl_usb_cdc_acm);
PBL_DEVICE_INSTANCE(pbl_usb_cdc_acm, struct pbl_usb_cdc_acm, struct pbl_uart, uart){
  .uart = PBL_UART_INIT(pbl_usb_cdc_acm, "usb_acm", nullptr, &s_ops, 115200),
};
