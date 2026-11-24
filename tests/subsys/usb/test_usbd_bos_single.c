/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_udc.h"

// A single WinUSB interface: the device is not composite, and the set has no subsets
static const uint8_t s_vendor_desc[] = {
  9, PBL_USB_DESC_INTERFACE, 0, 0, 0, 0xff, 0, 0, 0,
};

PBL_USBD_FUNCTION_DEFINE(vendor) = {
  .desc = s_vendor_desc,
  .desc_len = sizeof(s_vendor_desc),
  .first_iface = 0U,
  .num_ifaces = 1U,
  .winusb_guid = GUID,
};

void test_usbd_bos_single__initialize(void) {
  cl_assert_equal_i(pbl_usbd_enable(), 0);
}

void test_usbd_bos_single__common(void) {
  prv_assert_common();
}

void test_usbd_bos_single__bos(void) {
  prv_assert_bos(162U);
}

void test_usbd_bos_single__descriptor_set(void) {
  static const uint8_t header[] = {10, 0, 0, 0, 0x00, 0x00, 0x03, 0x06, 162, 0};

  cl_assert_equal_i(prv_vendor(0x01, 0U, 7U), 162);
  cl_assert_equal_m(s_in, header, sizeof(header));
  prv_assert_winusb(sizeof(header));
}
