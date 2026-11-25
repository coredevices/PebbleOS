/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_udc.h"

// A two-interface function without WinUSB, then a WinUSB one: a composite device
static const struct PBL_PACKED {
  struct pbl_usb_desc_iface comm;
  struct pbl_usb_desc_iface data;
} s_serial_desc = {
  .comm =
      {
        .bLength = sizeof(struct pbl_usb_desc_iface),
        .bDescriptorType = PBL_USB_DESC_INTERFACE,
        .bInterfaceNumber = 0U,
        .bInterfaceClass = 0x02U,
        .bInterfaceSubClass = 0x02U,
        .bInterfaceProtocol = 0x01U,
      },
  .data = {
    .bLength = sizeof(struct pbl_usb_desc_iface),
    .bDescriptorType = PBL_USB_DESC_INTERFACE,
    .bInterfaceNumber = 1U,
    .bInterfaceClass = 0x0aU,
  },
};

static const struct pbl_usb_desc_iface s_vendor_desc = {
  .bLength = sizeof(struct pbl_usb_desc_iface),
  .bDescriptorType = PBL_USB_DESC_INTERFACE,
  .bInterfaceNumber = 0U,
  .bInterfaceClass = 0xffU,
};

PBL_USBD_FUNCTION_DEFINE(a_serial) = {
  .desc = &s_serial_desc,
  .desc_len = sizeof(s_serial_desc),
};

PBL_USBD_FUNCTION_DEFINE(b_vendor) = {
  .desc = &s_vendor_desc,
  .desc_len = sizeof(s_vendor_desc),
  .winusb_guid = GUID,
};

void test_usbd_bos__initialize(void) {
  cl_assert_equal_i(pbl_usbd_enable(), 0);
}

void test_usbd_bos__common(void) {
  prv_assert_common();
}

void test_usbd_bos__bos(void) {
  prv_assert_bos(178U);
}

void test_usbd_bos__descriptor_set(void) {
  static const uint8_t header[] = {
    // Set header: Windows 8.1, 178 bytes
    10,
    0,
    0,
    0,
    0x00,
    0x00,
    0x03,
    0x06,
    178,
    0,
    // Configuration subset, of the first configuration
    8,
    0,
    1,
    0,
    0,
    0,
    168,
    0,
    // Function subset, from interface 2
    8,
    0,
    2,
    0,
    2,
    0,
    160,
    0,
  };

  cl_assert_equal_i(prv_vendor(0x01, 0U, 7U), 178);
  cl_assert_equal_m(s_in, header, sizeof(header));
  prv_assert_winusb(sizeof(header));
}

void test_usbd_bos__short_read(void) {
  cl_assert_equal_i(prv_request_in(PBL_USB_TYPE_VENDOR | PBL_USB_RECIP_DEVICE, 0x01, 0U, 7U, 10U),
                    10);
  cl_assert_equal_i(prv_le16(8U), 178);
}
