/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <pbl/drivers/udc.h>
#include <pbl/usb/usb.h>
#include <pbl/usb/usbd.h>

#include <clar.h>

// A controller of FAKE_NUM_EPS endpoints, with FAKE_CAPS
#ifndef FAKE_NUM_EPS
#define FAKE_NUM_EPS 8U
#endif
#ifndef FAKE_CAPS
#define FAKE_CAPS 0U
#endif

static const struct pbl_udc s_udc = {
  .num_eps = FAKE_NUM_EPS,
  .max_speed = PBL_UDC_SPEED_FULL,
  .caps = FAKE_CAPS,
};
const struct pbl_udc *const UDC = &s_udc;

static const struct pbl_udc_callbacks *s_cb;
static uint8_t s_in[1024];
static size_t s_in_len;
static bool s_stalled;
static bool s_connected;
// Enabled endpoints, by number, OUT then IN
static bool s_enabled_eps[2][16];
// Length of the last transfer queued on each endpoint, by number, OUT then IN; -1 for none
static int s_queued[2][16];

int pbl_device_init(const struct pbl_device *dev) {
  return 0;
}

void pbl_udc_register(const struct pbl_udc *udc, const struct pbl_udc_callbacks *cb, void *arg) {
  s_cb = cb;
}

int pbl_udc_enable(const struct pbl_udc *udc) {
  s_connected = true;
  return 0;
}

enum pbl_udc_speed pbl_udc_speed(const struct pbl_udc *udc) {
  return PBL_UDC_SPEED_FULL;
}

int pbl_udc_set_address(const struct pbl_udc *udc, uint8_t addr) {
  return 0;
}

int pbl_udc_ep_enable(const struct pbl_udc *udc, uint8_t ep, enum pbl_udc_ep_type type,
                      uint16_t mps) {
  bool in = (ep & PBL_UDC_EP_IN) != 0U;
  uint8_t n = ep & ~PBL_UDC_EP_IN;

  cl_assert(n < FAKE_NUM_EPS);
  if (n != 0U && (FAKE_CAPS & PBL_UDC_CAP_EP_ONE_DIR) != 0U) {
    cl_assert(!s_enabled_eps[!in][n]);
  }
  s_enabled_eps[in][n] = true;
  return 0;
}

int pbl_udc_ep_disable(const struct pbl_udc *udc, uint8_t ep) {
  s_enabled_eps[(ep & PBL_UDC_EP_IN) != 0U][ep & ~PBL_UDC_EP_IN] = false;
  return 0;
}

int pbl_udc_ep_set_halt(const struct pbl_udc *udc, uint8_t ep, bool halt) {
  s_stalled |= halt;
  return 0;
}

int pbl_udc_ep_queue(const struct pbl_udc *udc, uint8_t ep, void *buf, size_t len) {
  if ((ep & ~PBL_UDC_EP_IN) != 0U) {
    s_queued[(ep & PBL_UDC_EP_IN) != 0U][ep & ~PBL_UDC_EP_IN] = (int)len;
  }
  if (ep == PBL_UDC_EP_IN && len != 0U) {
    cl_assert(len <= sizeof(s_in));
    memcpy(s_in, buf, len);
    s_in_len = len;
  }
  return 0;
}

// A control request: the length of an IN data stage, or -1 when the device stalls
static int prv_request(uint8_t type, uint8_t req, uint16_t value, uint16_t index, uint16_t length) {
  uint8_t setup[8] = {
    type,
    req,
    (uint8_t)value,
    (uint8_t)(value >> 8),
    (uint8_t)index,
    (uint8_t)(index >> 8),
    (uint8_t)length,
    (uint8_t)(length >> 8),
  };

  s_in_len = 0U;
  s_stalled = false;
  cl_assert(s_cb != nullptr);
  s_cb->event(UDC, PBL_UDC_EVENT_SETUP, setup, nullptr);

  return s_stalled ? -1 : (int)s_in_len;
}

[[maybe_unused]] static int prv_request_in(uint8_t type, uint8_t req, uint16_t value,
                                           uint16_t index, uint16_t length) {
  return prv_request(PBL_USB_DIR_IN | type, req, value, index, length);
}

[[maybe_unused]] static int prv_get_descriptor(uint8_t type, uint8_t index) {
  return prv_request_in(PBL_USB_TYPE_STANDARD | PBL_USB_RECIP_DEVICE, PBL_USB_REQ_GET_DESCRIPTOR,
                        (uint16_t)((type << 8) | index), 0U, 0xffffU);
}

[[maybe_unused]] static int prv_vendor(uint8_t req, uint16_t value, uint16_t index) {
  return prv_request_in(PBL_USB_TYPE_VENDOR | PBL_USB_RECIP_DEVICE, req, value, index, 0xffffU);
}

[[maybe_unused]] static uint16_t prv_le16(size_t off) {
  return (uint16_t)(s_in[off] | (s_in[off + 1U] << 8));
}

// The UTF-16LE string of len bytes at off, as ASCII, its null characters as '|'
[[maybe_unused]] static void prv_assert_utf16(size_t off, size_t len, const char *expected) {
  char str[128] = {0};

  cl_assert(len / 2U < sizeof(str));
  for (size_t i = 0U; i < len / 2U; i++) {
    cl_assert_equal_i(s_in[off + 2U * i + 1U], 0);
    str[i] = s_in[off + 2U * i] != 0U ? (char)s_in[off + 2U * i] : '|';
  }
  cl_assert_equal_s(str, expected);
}

[[maybe_unused]] static const uint8_t s_ms_os_20_uuid[16] = {
  0xdf, 0x60, 0xdd, 0xd8, 0x89, 0x45, 0xc7, 0x4c, 0x9c, 0xd2, 0x65, 0x9d, 0x9e, 0x64, 0x8a, 0x9f,
};

[[maybe_unused]] static const uint8_t s_webusb_uuid[16] = {
  0x38, 0xb6, 0x08, 0x34, 0xa9, 0x09, 0xa0, 0x47, 0x8b, 0xfd, 0xa0, 0x76, 0x88, 0x15, 0xb6, 0x65,
};

#define GUID "{DE07B10A-24B5-440C-A638-F3855BEB7979}"

// The compatible ID and registry property of a WinUSB function, at off
[[maybe_unused]] static void prv_assert_winusb(size_t off) {
  static const uint8_t compat[20] = {20, 0, 3, 0, 'W', 'I', 'N', 'U', 'S', 'B'};

  cl_assert_equal_m(&s_in[off], compat, sizeof(compat));
  off += sizeof(compat);

  cl_assert_equal_i(prv_le16(off), 132);
  cl_assert_equal_i(prv_le16(off + 2U), 4);
  cl_assert_equal_i(prv_le16(off + 4U), 7);
  cl_assert_equal_i(prv_le16(off + 6U), 42);
  prv_assert_utf16(off + 8U, 42U, "DeviceInterfaceGUIDs|");
  cl_assert_equal_i(prv_le16(off + 50U), 80);
  prv_assert_utf16(off + 52U, 80U, GUID "||");
}

// BOS with the USB 2.0 extension, MS OS 2.0 for a set of set_len bytes, and WebUSB
[[maybe_unused]] static void prv_assert_bos(uint16_t set_len) {
  cl_assert_equal_i(prv_get_descriptor(PBL_USB_DESC_BOS, 0U), 64);
  cl_assert_equal_i(s_in[0], 5);
  cl_assert_equal_i(s_in[1], 0x0f);
  cl_assert_equal_i(prv_le16(2U), 64);
  cl_assert_equal_i(s_in[4], 3);

  static const uint8_t usb20_ext[7] = {7, 0x10, 0x02, 0, 0, 0, 0};
  cl_assert_equal_m(&s_in[5], usb20_ext, sizeof(usb20_ext));

  cl_assert_equal_i(s_in[12], 28);
  cl_assert_equal_i(s_in[13], 0x10);
  cl_assert_equal_i(s_in[14], 0x05);
  cl_assert_equal_i(s_in[15], 0);
  cl_assert_equal_m(&s_in[16], s_ms_os_20_uuid, 16U);
  cl_assert_equal_i(prv_le16(32U), 0x0000);
  cl_assert_equal_i(prv_le16(34U), 0x0603);
  cl_assert_equal_i(prv_le16(36U), set_len);
  cl_assert_equal_i(s_in[38], 0x01);
  cl_assert_equal_i(s_in[39], 0);

  cl_assert_equal_i(s_in[40], 24);
  cl_assert_equal_i(s_in[41], 0x10);
  cl_assert_equal_i(s_in[42], 0x05);
  cl_assert_equal_i(s_in[43], 0);
  cl_assert_equal_m(&s_in[44], s_webusb_uuid, 16U);
  cl_assert_equal_i(prv_le16(60U), 0x0100);
  cl_assert_equal_i(s_in[62], 0x02);
  cl_assert_equal_i(s_in[63], 1);
}

[[maybe_unused]] static void prv_assert_common(void) {
  static const uint8_t url[] = {18,  3,   1,   'e', 'x', 'a', 'm', 'p', 'l',
                                'e', '.', 'c', 'o', 'm', '/', 'u', 's', 'b'};

  cl_assert_equal_i(prv_get_descriptor(PBL_USB_DESC_DEVICE, 0U), 18);
  cl_assert_equal_i(prv_le16(2U), 0x0210);

  cl_assert_equal_i(prv_vendor(0x02, 1U, 2U), sizeof(url));
  cl_assert_equal_m(s_in, url, sizeof(url));
  cl_assert_equal_i(prv_vendor(0x02, 2U, 2U), -1);
  cl_assert_equal_i(prv_vendor(0x01, 0U, 8U), -1);
  cl_assert_equal_i(prv_vendor(0x03, 0U, 7U), -1);
}
