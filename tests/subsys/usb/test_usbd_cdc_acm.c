/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Two serial ports on a controller of 8 endpoints whose numbers each serve one direction
#define FAKE_NUM_EPS 8U
#define FAKE_CAPS    PBL_UDC_CAP_EP_ONE_DIR

#include "fake_udc.h"

#include <stddef.h>

#include <pbl/usb/cdc_acm.h>

// Offsets in the configuration descriptor and in the descriptors of a port
#define PORT_LEN        66U
#define OFF_PORT(n)     (9U + PORT_LEN * (n))
#define OFF_IAD_FIRST   2U
#define OFF_IAD_STRING  7U
#define OFF_COMM_IFACE  10U
#define OFF_CALL_DATA   26U
#define OFF_UNION_CTRL  34U
#define OFF_UNION_SUB   35U
#define OFF_NOTIFY_ADDR 38U
#define OFF_DATA_IFACE  45U
#define OFF_DATA_OUT_EP 54U
#define OFF_DATA_IN_EP  61U

static const struct pbl_uart *s_rx_uart;
static unsigned int s_rx_count;

bool pbl_device_is_ready(const struct pbl_device *dev) {
  return true;
}

static void prv_rx(const struct pbl_uart *uart, uint8_t c, void *arg) {
  s_rx_uart = uart;
  s_rx_count++;
}

void test_usbd_cdc_acm__initialize(void) {
  cl_assert_equal_i(pbl_usbd_enable(), 0);
  s_rx_uart = nullptr;
  s_rx_count = 0U;
}

void test_usbd_cdc_acm__config_desc(void) {
  cl_assert_equal_i(prv_get_descriptor(PBL_USB_DESC_CONFIGURATION, 0U), 9U + 2U * PORT_LEN);
  cl_assert_equal_i(s_in[offsetof(struct pbl_usb_desc_config, bNumInterfaces)], 4);

  for (uint8_t n = 0U; n < 2U; n++) {
    size_t off = OFF_PORT(n);

    cl_assert_equal_i(s_in[off + 1U], PBL_USB_DESC_IAD);
    cl_assert_equal_i(s_in[off + OFF_IAD_FIRST], 2U * n);
    cl_assert_equal_i(s_in[off + OFF_COMM_IFACE], 2U * n);
    cl_assert_equal_i(s_in[off + OFF_CALL_DATA], 2U * n + 1U);
    cl_assert_equal_i(s_in[off + OFF_UNION_CTRL], 2U * n);
    cl_assert_equal_i(s_in[off + OFF_UNION_SUB], 2U * n + 1U);
    cl_assert_equal_i(s_in[off + OFF_DATA_IFACE], 2U * n + 1U);
    cl_assert_equal_i(s_in[off + OFF_NOTIFY_ADDR], 0x81U + 3U * n);
    cl_assert_equal_i(s_in[off + OFF_DATA_OUT_EP], 0x02U + 3U * n);
    cl_assert_equal_i(s_in[off + OFF_DATA_IN_EP], 0x83U + 3U * n);
  }
}

void test_usbd_cdc_acm__names(void) {
  uint8_t index[2];
  int len;

  cl_assert(prv_get_descriptor(PBL_USB_DESC_CONFIGURATION, 0U) > 0);
  index[0] = s_in[OFF_PORT(0U) + OFF_IAD_STRING];
  index[1] = s_in[OFF_PORT(1U) + OFF_IAD_STRING];
  cl_assert(index[0] != index[1]);

  len = prv_get_descriptor(PBL_USB_DESC_STRING, index[0]);
  prv_assert_utf16(2U, (size_t)len - 2U, "Serial port 0");
  len = prv_get_descriptor(PBL_USB_DESC_STRING, index[1]);
  prv_assert_utf16(2U, (size_t)len - 2U, "Serial port 1");
}

void test_usbd_cdc_acm__requests(void) {
  uint8_t type = PBL_USB_TYPE_CLASS | PBL_USB_RECIP_INTERFACE;

  cl_assert_equal_i(prv_request_in(type, 0x21, 0U, 0U, 7U), 7);
  cl_assert_equal_i(prv_request_in(type, 0x21, 0U, 2U, 7U), 7);
  cl_assert_equal_i(prv_request_in(type, 0x21, 0U, 4U, 7U), -1);
}

void test_usbd_cdc_acm__receive(void) {
  cl_assert_equal_i(pbl_uart_rx_enable(USB_CDC_ACM_UART(1), prv_rx, nullptr), 0);

  s_cb->xfer_done(UDC, 0x05, 3, nullptr);
  cl_assert(s_rx_uart == USB_CDC_ACM_UART(1));
  cl_assert_equal_i(s_rx_count, 3);

  s_cb->xfer_done(UDC, 0x02, 2, nullptr);
  cl_assert_equal_i(s_rx_count, 3);
}

void test_usbd_cdc_acm__dtr(void) {
  uint8_t type = PBL_USB_TYPE_CLASS | PBL_USB_RECIP_INTERFACE;

  memset(s_queued, 0xff, sizeof(s_queued));
  cl_assert_equal_i(prv_request(PBL_USB_TYPE_STANDARD, PBL_USB_REQ_SET_CONFIGURATION, 1U, 0U, 0U),
                    0);
  cl_assert(!pbl_usb_cdc_acm_dtr(USB_CDC_ACM_UART(1)));

  // Held while the host has not opened the port
  pbl_uart_poll_out(USB_CDC_ACM_UART(1), 'a');
  pbl_uart_poll_out(USB_CDC_ACM_UART(1), 'b');
  pbl_uart_poll_out(USB_CDC_ACM_UART(1), 'c');
  cl_assert_equal_i(s_queued[1][6], -1);

  // DTR on port 1 sends what it held, on its own IN endpoint
  cl_assert_equal_i(prv_request(type, 0x22, 0x0001U, 2U, 0U), 0);
  cl_assert(pbl_usb_cdc_acm_dtr(USB_CDC_ACM_UART(1)));
  cl_assert(!pbl_usb_cdc_acm_dtr(USB_CDC_ACM_UART(0)));
  cl_assert_equal_i(s_queued[1][6], 3);
  cl_assert_equal_i(s_queued[1][3], -1);
}
