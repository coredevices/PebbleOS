/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/udc.h>
#include <pbl/kernel/msgq.h>
#include <pbl/kernel/time.h>
#include <pbl/usb/usbd.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/size.h>

#define RUN_S 60

#define IFACE 0U

#define EP_OUT 0x01U
#define EP_IN  (PBL_UDC_EP_IN | 2U)

#define ALT_OFF  0U
#define ALT_BULK 1U
#define ALT_INT  2U

#define INT_MPS 16U

#define VENDOR_CLASS 0xffU

#define STRING_OFF  1U
#define STRING_BULK 2U
#define STRING_INT  3U

static const struct PBL_PACKED {
  /* Setting 0, without endpoints */
  struct pbl_usb_desc_iface off;
  /* Setting 1, looping back on bulk endpoints */
  struct pbl_usb_desc_iface bulk;
  struct pbl_usb_desc_ep bulk_out;
  struct pbl_usb_desc_ep bulk_in;
  /* Setting 2, on the same endpoints as interrupt ones */
  struct pbl_usb_desc_iface intr;
  struct pbl_usb_desc_ep intr_out;
  struct pbl_usb_desc_ep intr_in;
} s_desc = {
  .off =
      {
        .bLength = sizeof(struct pbl_usb_desc_iface),
        .bDescriptorType = PBL_USB_DESC_INTERFACE,
        .bInterfaceNumber = IFACE,
        .bAlternateSetting = ALT_OFF,
        .bNumEndpoints = 0U,
        .bInterfaceClass = VENDOR_CLASS,
        .iInterface = STRING_OFF,
      },
  .bulk =
      {
        .bLength = sizeof(struct pbl_usb_desc_iface),
        .bDescriptorType = PBL_USB_DESC_INTERFACE,
        .bInterfaceNumber = IFACE,
        .bAlternateSetting = ALT_BULK,
        .bNumEndpoints = 2U,
        .bInterfaceClass = VENDOR_CLASS,
        .iInterface = STRING_BULK,
      },
  .bulk_out =
      {
        .bLength = sizeof(struct pbl_usb_desc_ep),
        .bDescriptorType = PBL_USB_DESC_ENDPOINT,
        .bEndpointAddress = EP_OUT,
        .bmAttributes = PBL_UDC_EP_BULK,
        .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
      },
  .bulk_in =
      {
        .bLength = sizeof(struct pbl_usb_desc_ep),
        .bDescriptorType = PBL_USB_DESC_ENDPOINT,
        .bEndpointAddress = EP_IN,
        .bmAttributes = PBL_UDC_EP_BULK,
        .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
      },
  .intr =
      {
        .bLength = sizeof(struct pbl_usb_desc_iface),
        .bDescriptorType = PBL_USB_DESC_INTERFACE,
        .bInterfaceNumber = IFACE,
        .bAlternateSetting = ALT_INT,
        .bNumEndpoints = 2U,
        .bInterfaceClass = VENDOR_CLASS,
        .iInterface = STRING_INT,
      },
  .intr_out =
      {
        .bLength = sizeof(struct pbl_usb_desc_ep),
        .bDescriptorType = PBL_USB_DESC_ENDPOINT,
        .bEndpointAddress = EP_OUT,
        .bmAttributes = PBL_UDC_EP_INTERRUPT,
        .wMaxPacketSize = PBL_CPU_TO_LE16(INT_MPS),
        .bInterval = 1U,
      },
  .intr_in = {
    .bLength = sizeof(struct pbl_usb_desc_ep),
    .bDescriptorType = PBL_USB_DESC_ENDPOINT,
    .bEndpointAddress = EP_IN,
    .bmAttributes = PBL_UDC_EP_INTERRUPT,
    .wMaxPacketSize = PBL_CPU_TO_LE16(INT_MPS),
    .bInterval = 1U,
  },
};

static const char *const s_strings[] = {
  "Loopback off",
  "Loopback bulk",
  "Loopback interrupt",
};

/* The stack's callbacks run in the controller's interrupt: they queue, the main thread prints. */
static PBL_MSGQ_DEFINE(s_alts, sizeof(uint8_t), 8);

static uint8_t s_alt;
static uint8_t s_buf[512];
static atomic_uint s_looped;

static uint16_t prv_mps(void) {
  return s_alt == ALT_BULK ? pbl_usbd_bulk_mps() : INT_MPS;
}

static void prv_rx_start(void) {
  (void)pbl_udc_ep_queue(UDC, EP_OUT, s_buf, prv_mps());
}

static void prv_set_alt(uint8_t iface, uint8_t alt) {
  s_alt = alt;
  if (alt != ALT_OFF) {
    prv_rx_start();
  }
  (void)pbl_msgq_put(&s_alts, &alt, PBL_NO_WAIT);
}

static void prv_configured(bool on) {
  s_alt = ALT_OFF;
}

/* What arrives goes back, then the next packet is read. */
static void prv_xfer_done(uint8_t ep, int ret) {
  if (ret < 0) {
    return;
  }

  if (ep == EP_OUT) {
    atomic_fetch_add(&s_looped, (unsigned int)ret);
    (void)pbl_udc_ep_queue(UDC, EP_IN, s_buf, (size_t)ret);
  } else {
    prv_rx_start();
  }
}

PBL_USBD_FUNCTION_DEFINE(loopback) = {
  .desc = &s_desc,
  .desc_len = sizeof(s_desc),
  .strings = s_strings,
  .num_strings = PBL_ARRAY_SIZE(s_strings),
  .first_iface = IFACE,
  .num_ifaces = 1U,
  .configured = prv_configured,
  .set_alt = prv_set_alt,
  .xfer_done = prv_xfer_done,
};

int main(void) {
  uint64_t end = pbl_uptime_ms() + RUN_S * 1000U;
  uint8_t alt;
  int ret;

  ret = pbl_usbd_enable();
  if (ret != 0) {
    fprintf(stderr, "usb: device failed (%d)\n", ret);
    return ret;
  }

  printf("usb: loopback up for %d s\n", RUN_S);

  while (pbl_uptime_ms() < end) {
    if (pbl_msgq_get(&s_alts, &alt, PBL_MSEC(100)) == 0) {
      printf("usb: alternate setting %u, %s\n", alt, s_strings[alt]);
    }
  }

  printf("usb: looped %u bytes\n", atomic_load(&s_looped));
  printf("Sample done\n");

  return 0;
}
