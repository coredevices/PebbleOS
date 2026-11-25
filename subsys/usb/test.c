/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/drivers/udc.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/irq.h>
#include <pbl/usb/usbd.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/math.h>

#define IFACE 0U

#define EP_LOOP_OUT 1U
#define EP_LOOP_IN  (PBL_UDC_EP_IN | 2U)
#define EP_SINK     3U
#define EP_SOURCE   (PBL_UDC_EP_IN | 4U)
#define EP_INT      (PBL_UDC_EP_IN | 5U)

#define VENDOR_CLASS 0xffU

#define INT_MPS      16U
#define INT_INTERVAL 1U

#define REQ_WRITE 0x01U
#define REQ_READ  0x02U
#define REQ_STALL 0x03U
#define REQ_RESET 0x04U
#define REQ_STATS 0x05U

#define CTRL_LEN 1024U
// 4 KiB, and room for the short packet that ends it at high speed
#define LOOP_LEN   4608U
#define STREAM_LEN 512U

enum prv_stat {
  STAT_LOOP_BYTES,
  STAT_SOURCE_BYTES,
  STAT_SINK_BYTES,
  STAT_SINK_ERRORS,
  STAT_INT_PACKETS,
  STAT_COUNT,
};

static const struct PBL_PACKED {
  struct pbl_usb_desc_iface iface;
  struct pbl_usb_desc_ep loop_out;
  struct pbl_usb_desc_ep loop_in;
  struct pbl_usb_desc_ep sink;
  struct pbl_usb_desc_ep source;
  struct pbl_usb_desc_ep intr;
} s_desc = {
  .iface =
      {
        .bLength = sizeof(struct pbl_usb_desc_iface),
        .bDescriptorType = PBL_USB_DESC_INTERFACE,
        .bInterfaceNumber = IFACE,
        .bNumEndpoints = 5U,
        .bInterfaceClass = VENDOR_CLASS,
      },
  .loop_out =
      {
        .bLength = sizeof(struct pbl_usb_desc_ep),
        .bDescriptorType = PBL_USB_DESC_ENDPOINT,
        .bEndpointAddress = EP_LOOP_OUT,
        .bmAttributes = PBL_UDC_EP_BULK,
        .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
      },
  .loop_in =
      {
        .bLength = sizeof(struct pbl_usb_desc_ep),
        .bDescriptorType = PBL_USB_DESC_ENDPOINT,
        .bEndpointAddress = EP_LOOP_IN,
        .bmAttributes = PBL_UDC_EP_BULK,
        .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
      },
  .sink =
      {
        .bLength = sizeof(struct pbl_usb_desc_ep),
        .bDescriptorType = PBL_USB_DESC_ENDPOINT,
        .bEndpointAddress = EP_SINK,
        .bmAttributes = PBL_UDC_EP_BULK,
        .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
      },
  .source =
      {
        .bLength = sizeof(struct pbl_usb_desc_ep),
        .bDescriptorType = PBL_USB_DESC_ENDPOINT,
        .bEndpointAddress = EP_SOURCE,
        .bmAttributes = PBL_UDC_EP_BULK,
        .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
      },
  .intr = {
    .bLength = sizeof(struct pbl_usb_desc_ep),
    .bDescriptorType = PBL_USB_DESC_ENDPOINT,
    .bEndpointAddress = EP_INT,
    .bmAttributes = PBL_UDC_EP_INTERRUPT,
    .wMaxPacketSize = PBL_CPU_TO_LE16(INT_MPS),
    .bInterval = INT_INTERVAL,
  },
};

PBL_USBD_FUNCTION_DECLARE(test);

static struct {
  bool configured;
  // Sequence number of the next packet, which its bytes count up from
  uint8_t source_seq;
  uint8_t sink_seq;
  // Of the interrupt packets, which REQ_RESET leaves alone: the next one may be queued already
  uint32_t int_seq;
  size_t loop_len;
  uint32_t stats[STAT_COUNT];
  uint8_t stats_le[4U * STAT_COUNT];
  uint8_t ctrl[CTRL_LEN];
  uint8_t loop[LOOP_LEN] PBL_ALIGNED(PBL_UDC_BUF_ALIGN);
  uint8_t source[STREAM_LEN] PBL_ALIGNED(PBL_UDC_BUF_ALIGN);
  uint8_t sink[STREAM_LEN] PBL_ALIGNED(PBL_UDC_BUF_ALIGN);
  uint8_t intr[INT_MPS] PBL_ALIGNED(PBL_UDC_BUF_ALIGN);
} s_test;

static uint8_t prv_ep(uint8_t ep) {
  return pbl_usbd_ep(PBL_USBD_FUNCTION(test), ep);
}

static void prv_loop_rx(void) {
  (void)pbl_udc_ep_queue(UDC, prv_ep(EP_LOOP_OUT), s_test.loop, sizeof(s_test.loop));
}

static void prv_sink_rx(void) {
  (void)pbl_udc_ep_queue(UDC, prv_ep(EP_SINK), s_test.sink, sizeof(s_test.sink));
}

static void prv_source_tx(void) {
  uint16_t mps = pbl_usbd_bulk_mps();

  for (size_t off = 0U; off < sizeof(s_test.source); off += mps) {
    for (uint16_t i = 0U; i < mps; i++) {
      s_test.source[off + i] = (uint8_t)(s_test.source_seq + i);
    }
    s_test.source_seq++;
  }

  (void)pbl_udc_ep_queue(UDC, prv_ep(EP_SOURCE), s_test.source, sizeof(s_test.source));
}

static void prv_int_tx(void) {
  pbl_put_le32(s_test.int_seq, s_test.intr);
  (void)pbl_udc_ep_queue(UDC, prv_ep(EP_INT), s_test.intr, sizeof(s_test.intr));
}

static void prv_sink_check(size_t len) {
  uint16_t mps = pbl_usbd_bulk_mps();

  for (size_t off = 0U; off < len; off += mps) {
    size_t chunk = PBL_MIN(len - off, (size_t)mps);

    for (size_t i = 0U; i < chunk; i++) {
      if (s_test.sink[off + i] != (uint8_t)(s_test.sink_seq + i)) {
        s_test.stats[STAT_SINK_ERRORS]++;
        break;
      }
    }
    s_test.sink_seq++;
  }

  s_test.stats[STAT_SINK_BYTES] += len;
}

// On the stack's thread, while the transfers end in the interrupt
static void prv_configured(bool on) {
  pbl_irq_lock();
  s_test.configured = on;
  if (on) {
    prv_loop_rx();
    prv_sink_rx();
    prv_source_tx();
    prv_int_tx();
  }
  pbl_irq_unlock();
}

static int prv_request(const struct pbl_usb_setup *setup, uint8_t **data, uint16_t *len) {
  bool in = (setup->bmRequestType & PBL_USB_DIR_IN) != 0U;

  if ((setup->bmRequestType & PBL_USB_TYPE_MASK) != PBL_USB_TYPE_VENDOR) {
    return -ENOTSUP;
  }

  switch (setup->bRequest) {
    case REQ_WRITE:
      if (in) {
        return -EINVAL;
      }
      *data = s_test.ctrl;
      *len = sizeof(s_test.ctrl);
      return 0;
    case REQ_READ:
      if (!in || setup->wValue > sizeof(s_test.ctrl)) {
        return -EINVAL;
      }
      *data = s_test.ctrl;
      *len = setup->wValue;
      return 0;
    case REQ_RESET:
      if (in) {
        return -EINVAL;
      }
      pbl_irq_lock();
      memset(s_test.stats, 0, sizeof(s_test.stats));
      s_test.sink_seq = 0U;
      pbl_irq_unlock();
      return 0;
    case REQ_STATS:
      if (!in) {
        return -EINVAL;
      }
      pbl_irq_lock();
      for (size_t i = 0U; i < STAT_COUNT; i++) {
        pbl_put_le32(s_test.stats[i], &s_test.stats_le[4U * i]);
      }
      pbl_irq_unlock();
      *data = s_test.stats_le;
      *len = sizeof(s_test.stats_le);
      return 0;
    case REQ_STALL:
    default:
      return -ENOTSUP;
  }
}

static void prv_xfer_done(uint8_t ep, int ret) {
  if (ret < 0 || !s_test.configured) {
    return;
  }

  switch (ep) {
    case EP_LOOP_OUT:
      s_test.loop_len = (size_t)ret;
      s_test.stats[STAT_LOOP_BYTES] += (uint32_t)ret;
      (void)pbl_udc_ep_queue(UDC, prv_ep(EP_LOOP_IN), s_test.loop, s_test.loop_len);
      break;
    case EP_LOOP_IN:
      // A transfer of full packets ends with a zero-length one
      if (s_test.loop_len != 0U && (s_test.loop_len % pbl_usbd_bulk_mps()) == 0U) {
        s_test.loop_len = 0U;
        (void)pbl_udc_ep_queue(UDC, prv_ep(EP_LOOP_IN), nullptr, 0U);
        break;
      }
      prv_loop_rx();
      break;
    case EP_SINK:
      prv_sink_check((size_t)ret);
      prv_sink_rx();
      break;
    case EP_SOURCE:
      s_test.stats[STAT_SOURCE_BYTES] += (uint32_t)ret;
      prv_source_tx();
      break;
    case EP_INT:
      s_test.int_seq++;
      s_test.stats[STAT_INT_PACKETS]++;
      prv_int_tx();
      break;
    default:
      break;
  }
}

PBL_USBD_FUNCTION_DEFINE(test) = {
  .desc = &s_desc,
  .desc_len = sizeof(s_desc),
  .configured = prv_configured,
  .class_request = prv_request,
  .xfer_done = prv_xfer_done,
};
