/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// A high-speed controller, running at either speed
#define FAKE_MAX_SPEED PBL_UDC_SPEED_HIGH

#include "fake_udc.h"

#include <stddef.h>

#include <pbl/util/byteorder.h>

static const struct PBL_PACKED {
  struct pbl_usb_desc_iface iface;
  struct pbl_usb_desc_ep bulk_out;
  struct pbl_usb_desc_ep bulk_in;
  struct pbl_usb_desc_ep intr;
} s_desc = {
  .iface =
      {
        .bLength = sizeof(struct pbl_usb_desc_iface),
        .bDescriptorType = PBL_USB_DESC_INTERFACE,
        .bNumEndpoints = 3U,
        .bInterfaceClass = 0xffU,
      },
  .bulk_out =
      {
        .bLength = sizeof(struct pbl_usb_desc_ep),
        .bDescriptorType = PBL_USB_DESC_ENDPOINT,
        .bEndpointAddress = 0x01U,
        .bmAttributes = PBL_UDC_EP_BULK,
        .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
      },
  .bulk_in =
      {
        .bLength = sizeof(struct pbl_usb_desc_ep),
        .bDescriptorType = PBL_USB_DESC_ENDPOINT,
        .bEndpointAddress = 0x81U,
        .bmAttributes = PBL_UDC_EP_BULK,
        .wMaxPacketSize = PBL_CPU_TO_LE16(64U),
      },
  .intr = {
    .bLength = sizeof(struct pbl_usb_desc_ep),
    .bDescriptorType = PBL_USB_DESC_ENDPOINT,
    .bEndpointAddress = 0x82U,
    .bmAttributes = PBL_UDC_EP_INTERRUPT,
    .wMaxPacketSize = PBL_CPU_TO_LE16(16U),
    .bInterval = 2U,
  },
};

PBL_USBD_FUNCTION_DEFINE(vendor) = {
  .desc = &s_desc,
  .desc_len = sizeof(s_desc),
};

#define CONFIG_LEN (sizeof(struct pbl_usb_desc_config) + sizeof(s_desc))
#define OFF(f)     (sizeof(struct pbl_usb_desc_config) + offsetof(__typeof__(s_desc), f))

// A configuration descriptor of the given type, with the packet sizes and interval of a speed
static void prv_assert_config(uint8_t type, enum pbl_udc_speed speed) {
  bool high = speed == PBL_UDC_SPEED_HIGH;

  cl_assert_equal_i(prv_get_descriptor(type, 0U), CONFIG_LEN);
  cl_assert_equal_i(s_in[0], sizeof(struct pbl_usb_desc_config));
  cl_assert_equal_i(s_in[1], type);
  cl_assert_equal_i(prv_le16(2U), CONFIG_LEN);
  cl_assert_equal_i(s_in[4], 1);
  cl_assert_equal_i(s_in[5], 1);

  cl_assert_equal_i(prv_le16(OFF(bulk_out.wMaxPacketSize)), high ? 512 : 64);
  cl_assert_equal_i(prv_le16(OFF(bulk_in.wMaxPacketSize)), high ? 512 : 64);
  cl_assert_equal_i(prv_le16(OFF(intr.wMaxPacketSize)), 16);
  // 2 frames, 16 microframes at high speed
  cl_assert_equal_i(s_in[OFF(intr.bInterval)], high ? 5 : 2);
}

void test_usbd_other_speed__initialize(void) {
  s_speed = PBL_UDC_SPEED_FULL;
  cl_assert_equal_i(pbl_usbd_enable(), 0);
}

void test_usbd_other_speed__high(void) {
  s_speed = PBL_UDC_SPEED_HIGH;
  prv_assert_config(PBL_USB_DESC_CONFIGURATION, PBL_UDC_SPEED_HIGH);
  prv_assert_config(PBL_USB_DESC_OTHER_SPEED, PBL_UDC_SPEED_FULL);
}

void test_usbd_other_speed__full(void) {
  s_speed = PBL_UDC_SPEED_FULL;
  prv_assert_config(PBL_USB_DESC_CONFIGURATION, PBL_UDC_SPEED_FULL);
  prv_assert_config(PBL_USB_DESC_OTHER_SPEED, PBL_UDC_SPEED_HIGH);
}

void test_usbd_other_speed__qualifier(void) {
  cl_assert_equal_i(prv_get_descriptor(PBL_USB_DESC_DEVICE_QUALIFIER, 0U), 10);
  cl_assert_equal_i(s_in[1], PBL_USB_DESC_DEVICE_QUALIFIER);
  cl_assert_equal_i(s_in[8], 1);
}

void test_usbd_other_speed__second_configuration(void) {
  cl_assert_equal_i(prv_get_descriptor(PBL_USB_DESC_OTHER_SPEED, 1U), -1);
}

// A configuration whose endpoints the controller cannot all take is not set
void test_usbd_other_speed__endpoint_refused(void) {
  uint8_t type = PBL_USB_TYPE_STANDARD | PBL_USB_RECIP_DEVICE;

  s_ep_refused = 0x82;
  cl_assert_equal_i(prv_request(type, PBL_USB_REQ_SET_CONFIGURATION, 1U, 0U, 0U), -1);
  cl_assert(!s_enabled_eps[0][1]);
  cl_assert(!s_enabled_eps[1][1]);
  cl_assert_equal_i(prv_request_in(type, PBL_USB_REQ_GET_CONFIGURATION, 0U, 0U, 1U), 1);
  cl_assert_equal_i(s_in[0], 0);

  s_ep_refused = -1;
  cl_assert_equal_i(prv_request(type, PBL_USB_REQ_SET_CONFIGURATION, 1U, 0U, 0U), 0);
  cl_assert(s_enabled_eps[0][1]);
  cl_assert(s_enabled_eps[1][1]);
  cl_assert(s_enabled_eps[1][2]);
}
