/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

/**
 * @defgroup usb USB
 * @ingroup subsys
 * @brief Definitions of the USB 2.0 specification the device stack uses.
 * @{
 */

/** @brief Setup packet, as it arrives, little-endian. */
struct pbl_usb_setup {
  /** Direction, type and recipient. */
  uint8_t bmRequestType;
  /** Request. */
  uint8_t bRequest;
  /** Value. */
  uint16_t wValue;
  /** Index: an interface or an endpoint. */
  uint16_t wIndex;
  /** Length of the data stage. */
  uint16_t wLength;
} __attribute__((packed));

/** @name bmRequestType fields */
/** @{ */
#define PBL_USB_DIR_IN          0x80U
#define PBL_USB_TYPE_MASK       0x60U
#define PBL_USB_TYPE_STANDARD   0x00U
#define PBL_USB_TYPE_CLASS      0x20U
#define PBL_USB_TYPE_VENDOR     0x40U
#define PBL_USB_RECIP_MASK      0x1fU
#define PBL_USB_RECIP_DEVICE    0x00U
#define PBL_USB_RECIP_INTERFACE 0x01U
#define PBL_USB_RECIP_ENDPOINT  0x02U
/** @} */

/** @name Standard requests */
/** @{ */
#define PBL_USB_REQ_GET_STATUS        0x00U
#define PBL_USB_REQ_CLEAR_FEATURE     0x01U
#define PBL_USB_REQ_SET_FEATURE       0x03U
#define PBL_USB_REQ_SET_ADDRESS       0x05U
#define PBL_USB_REQ_GET_DESCRIPTOR    0x06U
#define PBL_USB_REQ_GET_CONFIGURATION 0x08U
#define PBL_USB_REQ_SET_CONFIGURATION 0x09U
#define PBL_USB_REQ_GET_INTERFACE     0x0aU
#define PBL_USB_REQ_SET_INTERFACE     0x0bU
/** @} */

/** Feature selector of a halted endpoint. */
#define PBL_USB_FEATURE_ENDPOINT_HALT 0x00U

/** @name Descriptor types */
/** @{ */
#define PBL_USB_DESC_DEVICE           0x01U
#define PBL_USB_DESC_CONFIGURATION    0x02U
#define PBL_USB_DESC_STRING           0x03U
#define PBL_USB_DESC_INTERFACE        0x04U
#define PBL_USB_DESC_ENDPOINT         0x05U
#define PBL_USB_DESC_DEVICE_QUALIFIER 0x06U
#define PBL_USB_DESC_IAD              0x0bU
#define PBL_USB_DESC_CS_INTERFACE     0x24U
/** @} */

/** @} */
