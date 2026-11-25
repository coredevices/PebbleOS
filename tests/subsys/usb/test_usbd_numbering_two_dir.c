/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// A controller of 4 endpoints, each number in both directions: just enough for the functions
#define FAKE_NUM_EPS 4U
#define FAKE_CAPS    0U

#include "usbd_numbering.h"

void test_usbd_numbering_two_dir__initialize(void) {
  cl_assert_equal_i(pbl_usbd_enable(), 0);
}

void test_usbd_numbering_two_dir__endpoints(void) {
  cl_assert_equal_i(pbl_usbd_ep(PBL_USBD_FUNCTION(a_vendor), 0x01), 0x01);
  cl_assert_equal_i(pbl_usbd_ep(PBL_USBD_FUNCTION(a_vendor), 0x81), 0x81);
  cl_assert_equal_i(pbl_usbd_ep(PBL_USBD_FUNCTION(b_serial), 0x81), 0x82);
  cl_assert_equal_i(pbl_usbd_ep(PBL_USBD_FUNCTION(b_serial), 0x01), 0x02);
  cl_assert_equal_i(pbl_usbd_ep(PBL_USBD_FUNCTION(b_serial), 0x82), 0x83);
}

void test_usbd_numbering_two_dir__config_desc(void) {
  cl_assert_equal_i(prv_config_desc(), CONFIG_LEN);
  cl_assert_equal_i(CFG(OFF_VENDOR, s_vendor_desc, bulk_out.bEndpointAddress), 0x01);
  cl_assert_equal_i(CFG(OFF_VENDOR, s_vendor_desc, bulk_in.bEndpointAddress), 0x81);
  cl_assert_equal_i(CFG(OFF_SERIAL, s_serial_desc, notify.bEndpointAddress), 0x82);
  cl_assert_equal_i(CFG(OFF_SERIAL, s_serial_desc, data_out.bEndpointAddress), 0x02);
  cl_assert_equal_i(CFG(OFF_SERIAL, s_serial_desc, data_in.bEndpointAddress), 0x83);
  cl_assert_equal_i(s_in[OFF_DFU + offsetof(struct pbl_usb_desc_iface, bInterfaceNumber)], 3);
}
