/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Three functions numbered from 0 and 1 each, on the fake controller: a vendor one with alternate
// settings, a serial port and an endpoint-less one.

#pragma once

#include "fake_udc.h"

#include <errno.h>
#include <stddef.h>

#include <pbl/util/byteorder.h>

#define CDC_FUNC_CALL_MGMT 0x01U
#define CDC_FUNC_UNION     0x06U

struct PBL_PACKED cs_call_mgmt {
  uint8_t bFunctionLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bmCapabilities;
  uint8_t bDataInterface;
};

struct PBL_PACKED cs_union {
  uint8_t bFunctionLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubtype;
  uint8_t bControlInterface;
  uint8_t bSubordinateInterface0;
};

#define IFACE(num, alt, eps, cls)                 \
  {                                               \
    .bLength = sizeof(struct pbl_usb_desc_iface), \
    .bDescriptorType = PBL_USB_DESC_INTERFACE,    \
    .bInterfaceNumber = (num),                    \
    .bAlternateSetting = (alt),                   \
    .bNumEndpoints = (eps),                       \
    .bInterfaceClass = (cls),                     \
  }

#define EP(addr, type, mps, interval)          \
  {                                            \
    .bLength = sizeof(struct pbl_usb_desc_ep), \
    .bDescriptorType = PBL_USB_DESC_ENDPOINT,  \
    .bEndpointAddress = (addr),                \
    .bmAttributes = (type),                    \
    .wMaxPacketSize = PBL_CPU_TO_LE16(mps),    \
    .bInterval = (interval),                   \
  }

// Setting 0 without endpoints, settings 1 and 2 on the same pair
static const struct PBL_PACKED {
  struct pbl_usb_desc_iface off;
  struct pbl_usb_desc_iface bulk;
  struct pbl_usb_desc_ep bulk_out;
  struct pbl_usb_desc_ep bulk_in;
  struct pbl_usb_desc_iface intr;
  struct pbl_usb_desc_ep intr_out;
  struct pbl_usb_desc_ep intr_in;
} s_vendor_desc = {
  .off = IFACE(0U, 0U, 0U, 0xffU),
  .bulk = IFACE(0U, 1U, 2U, 0xffU),
  .bulk_out = EP(0x01U, PBL_UDC_EP_BULK, 64U, 0U),
  .bulk_in = EP(0x81U, PBL_UDC_EP_BULK, 64U, 0U),
  .intr = IFACE(0U, 2U, 2U, 0xffU),
  .intr_out = EP(0x01U, PBL_UDC_EP_INTERRUPT, 16U, 1U),
  .intr_in = EP(0x81U, PBL_UDC_EP_INTERRUPT, 16U, 1U),
};

static const struct PBL_PACKED {
  struct pbl_usb_desc_iad iad;
  struct pbl_usb_desc_iface comm;
  struct cs_call_mgmt call_mgmt;
  struct cs_union un;
  struct pbl_usb_desc_ep notify;
  struct pbl_usb_desc_iface data;
  struct pbl_usb_desc_ep data_out;
  struct pbl_usb_desc_ep data_in;
} s_serial_desc = {
  .iad =
      {
        .bLength = sizeof(struct pbl_usb_desc_iad),
        .bDescriptorType = PBL_USB_DESC_IAD,
        .bFirstInterface = 0U,
        .bInterfaceCount = 2U,
        .bFunctionClass = 0x02U,
        .bFunctionSubClass = 0x02U,
        .bFunctionProtocol = 0x01U,
      },
  .comm = IFACE(0U, 0U, 1U, 0x02U),
  .call_mgmt =
      {
        .bFunctionLength = sizeof(struct cs_call_mgmt),
        .bDescriptorType = PBL_USB_DESC_CS_INTERFACE,
        .bDescriptorSubtype = CDC_FUNC_CALL_MGMT,
        .bDataInterface = 1U,
      },
  .un =
      {
        .bFunctionLength = sizeof(struct cs_union),
        .bDescriptorType = PBL_USB_DESC_CS_INTERFACE,
        .bDescriptorSubtype = CDC_FUNC_UNION,
        .bControlInterface = 0U,
        .bSubordinateInterface0 = 1U,
      },
  .notify = EP(0x81U, PBL_UDC_EP_INTERRUPT, 16U, 16U),
  .data = IFACE(1U, 0U, 2U, 0x0aU),
  .data_out = EP(0x01U, PBL_UDC_EP_BULK, 64U, 0U),
  .data_in = EP(0x82U, PBL_UDC_EP_BULK, 64U, 0U),
};

static const struct pbl_usb_desc_iface s_dfu_desc = IFACE(0U, 0U, 0U, 0xfeU);

// Offsets in the configuration descriptor, after its own 9 bytes
#define OFF_VENDOR sizeof(struct pbl_usb_desc_config)
#define OFF_SERIAL (OFF_VENDOR + sizeof(s_vendor_desc))
#define OFF_DFU    (OFF_SERIAL + sizeof(s_serial_desc))
#define CONFIG_LEN (OFF_DFU + sizeof(s_dfu_desc))

static int s_set_alt_iface = -1;
static int s_set_alt_alt = -1;
static int s_xfer_ep = -1;
static int s_request_fn = -1;
static int s_request_iface = -1;

static void prv_vendor_set_alt(uint8_t iface, uint8_t alt) {
  s_set_alt_iface = iface;
  s_set_alt_alt = alt;
}

static void prv_serial_fixup(struct pbl_usb_desc_header *desc, uint8_t first_iface) {
  const struct cs_call_mgmt *cs = (const struct cs_call_mgmt *)desc;

  if (desc->bDescriptorType != PBL_USB_DESC_CS_INTERFACE) {
    return;
  }

  if (cs->bDescriptorSubtype == CDC_FUNC_CALL_MGMT) {
    ((struct cs_call_mgmt *)desc)->bDataInterface += first_iface;
  } else if (cs->bDescriptorSubtype == CDC_FUNC_UNION) {
    struct cs_union *u = (struct cs_union *)desc;

    u->bControlInterface += first_iface;
    u->bSubordinateInterface0 += first_iface;
  }
}

static void prv_serial_xfer_done(uint8_t ep, int ret) {
  s_xfer_ep = ep;
}

static int prv_serial_request(const struct pbl_usb_setup *setup, uint8_t **data, uint16_t *len) {
  s_request_fn = 1;
  s_request_iface = (uint8_t)setup->wIndex;
  return 0;
}

static int prv_dfu_request(const struct pbl_usb_setup *setup, uint8_t **data, uint16_t *len) {
  s_request_fn = 2;
  s_request_iface = (uint8_t)setup->wIndex;
  return 0;
}

PBL_USBD_FUNCTION_DEFINE(a_vendor) = {
  .desc = &s_vendor_desc,
  .desc_len = sizeof(s_vendor_desc),
  .set_alt = prv_vendor_set_alt,
};

PBL_USBD_FUNCTION_DEFINE(b_serial) = {
  .desc = &s_serial_desc,
  .desc_len = sizeof(s_serial_desc),
  .desc_fixup = prv_serial_fixup,
  .class_request = prv_serial_request,
  .xfer_done = prv_serial_xfer_done,
};

PBL_USBD_FUNCTION_DEFINE(c_dfu) = {
  .desc = &s_dfu_desc,
  .desc_len = sizeof(s_dfu_desc),
  .class_request = prv_dfu_request,
};

[[maybe_unused]] static int prv_config_desc(void) {
  return prv_get_descriptor(PBL_USB_DESC_CONFIGURATION, 0U);
}

// The configuration descriptor as the host gets it, at the offset of field f of a function's
#define CFG(base, desc, f) s_in[(base) + offsetof(__typeof__(desc), f)]
