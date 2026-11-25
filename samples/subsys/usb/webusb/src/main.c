/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/udc.h>
#include <pbl/kernel/delay.h>
#include <pbl/usb/usbd.h>
#include <pbl/util/byteorder.h>

#define ECHO_S  60
#define BUF_LEN 512U

#define IFACE  0U
#define EP_OUT 1U
#define EP_IN  (PBL_UDC_EP_IN | 1U)

#define VENDOR_CLASS 0xffU

static const struct PBL_PACKED {
  struct pbl_usb_desc_iface iface;
  struct pbl_usb_desc_ep out;
  struct pbl_usb_desc_ep in;
} s_desc = {
  .iface =
      {
        .bLength = sizeof(struct pbl_usb_desc_iface),
        .bDescriptorType = PBL_USB_DESC_INTERFACE,
        .bInterfaceNumber = IFACE,
        .bNumEndpoints = 2U,
        .bInterfaceClass = VENDOR_CLASS,
      },
  .out =
      {
        .bLength = sizeof(struct pbl_usb_desc_ep),
        .bDescriptorType = PBL_USB_DESC_ENDPOINT,
        .bEndpointAddress = EP_OUT,
        .bmAttributes = PBL_UDC_EP_BULK,
        .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
      },
  .in = {
    .bLength = sizeof(struct pbl_usb_desc_ep),
    .bDescriptorType = PBL_USB_DESC_ENDPOINT,
    .bEndpointAddress = EP_IN,
    .bmAttributes = PBL_UDC_EP_BULK,
    .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
  },
};

PBL_USBD_FUNCTION_DECLARE(webusb);

static atomic_bool s_configured;
static atomic_uint s_count;
static uint8_t s_buf[BUF_LEN];

static uint8_t prv_ep(uint8_t ep) {
  return pbl_usbd_ep(PBL_USBD_FUNCTION(webusb), ep);
}

/* A transfer ends with a short packet or a full buffer, and goes back as it came. */
static void prv_rx_start(void) {
  (void)pbl_udc_ep_queue(UDC, prv_ep(EP_OUT), s_buf, sizeof(s_buf));
}

static void prv_configured(bool on) {
  atomic_store(&s_configured, on);
  if (on) {
    prv_rx_start();
  }
}

static void prv_xfer_done(uint8_t ep, int ret) {
  if (ret < 0 || !atomic_load(&s_configured)) {
    return;
  }

  if (ep == EP_OUT && ret > 0) {
    atomic_fetch_add(&s_count, (unsigned int)ret);
    (void)pbl_udc_ep_queue(UDC, prv_ep(EP_IN), s_buf, (size_t)ret);
    return;
  }

  prv_rx_start();
}

PBL_USBD_FUNCTION_DEFINE(webusb) = {
  .desc = &s_desc,
  .desc_len = sizeof(s_desc),
  .configured = prv_configured,
  .xfer_done = prv_xfer_done,
  .winusb_guid = "{DE07B10A-24B5-440C-A638-F3855BEB7979}",
};

int main(void) {
  int ret;

  ret = pbl_usbd_enable();
  if (ret != 0) {
    fprintf(stderr, "usb: device failed (%d)\n", ret);
    return ret;
  }

  printf("usb: vendor interface up, echoing for %d s\n", ECHO_S);

  pbl_sleep_ms(ECHO_S * 1000U);

  printf("usb: echoed %u bytes\n", atomic_load(&s_count));
  printf("Sample done\n");

  return 0;
}
