/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// A controller of 8 endpoints whose numbers each serve one direction
#define FAKE_NUM_EPS 8U
#define FAKE_CAPS    PBL_UDC_CAP_EP_ONE_DIR

#include "usbd_numbering.h"

// Every endpoint of the configuration descriptor, IN or OUT, on a number of its own
static void prv_assert_one_dir(void) {
  bool used[2][16] = {0};

  for (size_t i = OFF_VENDOR; i < s_in_len; i += s_in[i]) {
    if (s_in[i + 1U] == PBL_USB_DESC_ENDPOINT) {
      used[(s_in[i + 2U] & PBL_UDC_EP_IN) != 0U][s_in[i + 2U] & 0x0fU] = true;
    }
  }
  for (size_t n = 1U; n < 16U; n++) {
    cl_assert(!(used[0][n] && used[1][n]));
  }
}

void test_usbd_numbering__initialize(void) {
  cl_assert_equal_i(pbl_usbd_enable(), 0);
  cl_assert(s_connected);
}

void test_usbd_numbering__endpoints(void) {
  cl_assert_equal_i(pbl_usbd_ep(PBL_USBD_FUNCTION(a_vendor), 0x01), 0x01);
  cl_assert_equal_i(pbl_usbd_ep(PBL_USBD_FUNCTION(a_vendor), 0x81), 0x82);
  cl_assert_equal_i(pbl_usbd_ep(PBL_USBD_FUNCTION(b_serial), 0x81), 0x83);
  cl_assert_equal_i(pbl_usbd_ep(PBL_USBD_FUNCTION(b_serial), 0x01), 0x04);
  cl_assert_equal_i(pbl_usbd_ep(PBL_USBD_FUNCTION(b_serial), 0x82), 0x85);
  cl_assert_equal_i(pbl_usbd_ep(PBL_USBD_FUNCTION(b_serial), 0x02), 0xff);
  cl_assert_equal_i(pbl_usbd_ep(PBL_USBD_FUNCTION(c_dfu), 0x01), 0xff);
}

void test_usbd_numbering__config_desc(void) {
  cl_assert_equal_i(prv_config_desc(), CONFIG_LEN);
  cl_assert_equal_i(s_in[offsetof(struct pbl_usb_desc_config, bNumInterfaces)], 4);

  // Vendor: interface 0, its three settings on endpoints 0x01 and 0x82
  cl_assert_equal_i(CFG(OFF_VENDOR, s_vendor_desc, off.bInterfaceNumber), 0);
  cl_assert_equal_i(CFG(OFF_VENDOR, s_vendor_desc, bulk.bInterfaceNumber), 0);
  cl_assert_equal_i(CFG(OFF_VENDOR, s_vendor_desc, bulk_out.bEndpointAddress), 0x01);
  cl_assert_equal_i(CFG(OFF_VENDOR, s_vendor_desc, bulk_in.bEndpointAddress), 0x82);
  cl_assert_equal_i(CFG(OFF_VENDOR, s_vendor_desc, intr.bInterfaceNumber), 0);
  cl_assert_equal_i(CFG(OFF_VENDOR, s_vendor_desc, intr_out.bEndpointAddress), 0x01);
  cl_assert_equal_i(CFG(OFF_VENDOR, s_vendor_desc, intr_in.bEndpointAddress), 0x82);

  // Serial: interfaces 1 and 2, in its association and its class-specific descriptors
  cl_assert_equal_i(CFG(OFF_SERIAL, s_serial_desc, iad.bDescriptorType), PBL_USB_DESC_IAD);
  cl_assert_equal_i(CFG(OFF_SERIAL, s_serial_desc, iad.bFirstInterface), 1);
  cl_assert_equal_i(CFG(OFF_SERIAL, s_serial_desc, comm.bInterfaceNumber), 1);
  cl_assert_equal_i(CFG(OFF_SERIAL, s_serial_desc, call_mgmt.bDataInterface), 2);
  cl_assert_equal_i(CFG(OFF_SERIAL, s_serial_desc, un.bControlInterface), 1);
  cl_assert_equal_i(CFG(OFF_SERIAL, s_serial_desc, un.bSubordinateInterface0), 2);
  cl_assert_equal_i(CFG(OFF_SERIAL, s_serial_desc, notify.bEndpointAddress), 0x83);
  cl_assert_equal_i(CFG(OFF_SERIAL, s_serial_desc, data.bInterfaceNumber), 2);
  cl_assert_equal_i(CFG(OFF_SERIAL, s_serial_desc, data_out.bEndpointAddress), 0x04);
  cl_assert_equal_i(CFG(OFF_SERIAL, s_serial_desc, data_in.bEndpointAddress), 0x85);

  // Firmware upgrade: interface 3
  cl_assert_equal_i(s_in[OFF_DFU + offsetof(struct pbl_usb_desc_iface, bInterfaceNumber)], 3);

  prv_assert_one_dir();
}

void test_usbd_numbering__requests(void) {
  uint8_t type = PBL_USB_TYPE_CLASS | PBL_USB_RECIP_INTERFACE;

  cl_assert_equal_i(prv_request(type, 0x22, 0U, 2U, 0U), 0);
  cl_assert_equal_i(s_request_fn, 1);
  cl_assert_equal_i(s_request_iface, 1);

  cl_assert_equal_i(prv_request(type, 0x03, 0U, 3U, 0U), 0);
  cl_assert_equal_i(s_request_fn, 2);
  cl_assert_equal_i(s_request_iface, 0);

  cl_assert_equal_i(prv_request(type, 0x03, 0U, 4U, 0U), -1);
}

void test_usbd_numbering__set_alt(void) {
  uint8_t type = PBL_USB_TYPE_STANDARD | PBL_USB_RECIP_INTERFACE;

  cl_assert_equal_i(prv_request(PBL_USB_TYPE_STANDARD, PBL_USB_REQ_SET_CONFIGURATION, 1U, 0U, 0U),
                    0);
  cl_assert(s_enabled_eps[1][3]);
  cl_assert(s_enabled_eps[0][4]);
  cl_assert(s_enabled_eps[1][5]);
  cl_assert(!s_enabled_eps[0][1]);

  cl_assert_equal_i(prv_request(type, PBL_USB_REQ_SET_INTERFACE, 1U, 0U, 0U), 0);
  cl_assert_equal_i(s_set_alt_iface, 0);
  cl_assert_equal_i(s_set_alt_alt, 1);
  cl_assert(s_enabled_eps[0][1]);
  cl_assert(s_enabled_eps[1][2]);

  cl_assert_equal_i(prv_request(type, PBL_USB_REQ_SET_INTERFACE, 3U, 0U, 0U), -1);
}

void test_usbd_numbering__xfer_done(void) {
  s_cb->xfer_done(UDC, 0x85, 10, nullptr);
  cl_assert_equal_i(s_xfer_ep, 0x82);

  s_cb->xfer_done(UDC, 0x04, 10, nullptr);
  cl_assert_equal_i(s_xfer_ep, 0x01);
}
