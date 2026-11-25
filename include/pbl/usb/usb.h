/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup usb USB
 * @ingroup subsys
 * @brief Definitions of the USB 2.0 specification the device stack uses.
 *
 * Descriptors are packed structures with the field names of the specification. Their multi-byte
 * fields are little-endian, on the bus as in memory: static initializers give them with
 * PBL_CPU_TO_LE16() and PBL_CPU_TO_LE32(), code with pbl_cpu_to_le16() and pbl_le16_to_cpu().
 * @{
 */

/** @brief Setup packet, its 16-bit fields in CPU order. */
struct PBL_PACKED pbl_usb_setup {
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
};

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
#define PBL_USB_DESC_OTHER_SPEED      0x07U
#define PBL_USB_DESC_IAD              0x0bU
#define PBL_USB_DESC_BOS              0x0fU
#define PBL_USB_DESC_DEVICE_CAP       0x10U
#define PBL_USB_DESC_CS_INTERFACE     0x24U
/** @} */

/** @name Device capability types, in the BOS descriptor */
/** @{ */
#define PBL_USB_CAP_USB20_EXT 0x02U
#define PBL_USB_CAP_PLATFORM  0x05U
/** @} */

/** @brief The first two fields of every descriptor. */
struct PBL_PACKED pbl_usb_desc_header {
  /** Length of the descriptor, in bytes. */
  uint8_t bLength;
  /** Descriptor type. */
  uint8_t bDescriptorType;
};

/** @brief Device descriptor. */
struct PBL_PACKED pbl_usb_desc_device {
  /** Length, 18. */
  uint8_t bLength;
  /** @ref PBL_USB_DESC_DEVICE. */
  uint8_t bDescriptorType;
  /** USB release, in binary-coded decimal. */
  uint16_t bcdUSB;
  /** Class. */
  uint8_t bDeviceClass;
  /** Subclass. */
  uint8_t bDeviceSubClass;
  /** Protocol. */
  uint8_t bDeviceProtocol;
  /** Maximum packet size of endpoint 0. */
  uint8_t bMaxPacketSize0;
  /** Vendor ID. */
  uint16_t idVendor;
  /** Product ID. */
  uint16_t idProduct;
  /** Device release, in binary-coded decimal. */
  uint16_t bcdDevice;
  /** Index of the manufacturer string. */
  uint8_t iManufacturer;
  /** Index of the product string. */
  uint8_t iProduct;
  /** Index of the serial number string. */
  uint8_t iSerialNumber;
  /** Number of configurations. */
  uint8_t bNumConfigurations;
};

/** @brief Device qualifier descriptor: the device as it would be at the other speed. */
struct PBL_PACKED pbl_usb_desc_qualifier {
  /** Length, 10. */
  uint8_t bLength;
  /** @ref PBL_USB_DESC_DEVICE_QUALIFIER. */
  uint8_t bDescriptorType;
  /** USB release, in binary-coded decimal. */
  uint16_t bcdUSB;
  /** Class. */
  uint8_t bDeviceClass;
  /** Subclass. */
  uint8_t bDeviceSubClass;
  /** Protocol. */
  uint8_t bDeviceProtocol;
  /** Maximum packet size of endpoint 0. */
  uint8_t bMaxPacketSize0;
  /** Number of configurations. */
  uint8_t bNumConfigurations;
  /** Reserved, 0. */
  uint8_t bReserved;
};

/** @brief Configuration descriptor, which the descriptors of its interfaces follow. */
struct PBL_PACKED pbl_usb_desc_config {
  /** Length, 9. */
  uint8_t bLength;
  /** @ref PBL_USB_DESC_CONFIGURATION, or @ref PBL_USB_DESC_OTHER_SPEED. */
  uint8_t bDescriptorType;
  /** Length of the configuration and the descriptors that follow it. */
  uint16_t wTotalLength;
  /** Number of interfaces. */
  uint8_t bNumInterfaces;
  /** Value of the configuration, for SET_CONFIGURATION. */
  uint8_t bConfigurationValue;
  /** Index of the configuration string. */
  uint8_t iConfiguration;
  /** Attributes. */
  uint8_t bmAttributes;
  /** Maximum power, in units of 2 mA. */
  uint8_t bMaxPower;
};

/** @brief Interface association descriptor: interfaces that make one function. */
struct PBL_PACKED pbl_usb_desc_iad {
  /** Length, 8. */
  uint8_t bLength;
  /** @ref PBL_USB_DESC_IAD. */
  uint8_t bDescriptorType;
  /** First interface. */
  uint8_t bFirstInterface;
  /** Number of interfaces. */
  uint8_t bInterfaceCount;
  /** Class. */
  uint8_t bFunctionClass;
  /** Subclass. */
  uint8_t bFunctionSubClass;
  /** Protocol. */
  uint8_t bFunctionProtocol;
  /** Index of the function string. */
  uint8_t iFunction;
};

/** @brief Interface descriptor, of one alternate setting of an interface. */
struct PBL_PACKED pbl_usb_desc_iface {
  /** Length, 9. */
  uint8_t bLength;
  /** @ref PBL_USB_DESC_INTERFACE. */
  uint8_t bDescriptorType;
  /** Interface number. */
  uint8_t bInterfaceNumber;
  /** Alternate setting. */
  uint8_t bAlternateSetting;
  /** Number of endpoints, endpoint 0 excluded. */
  uint8_t bNumEndpoints;
  /** Class. */
  uint8_t bInterfaceClass;
  /** Subclass. */
  uint8_t bInterfaceSubClass;
  /** Protocol. */
  uint8_t bInterfaceProtocol;
  /** Index of the interface string. */
  uint8_t iInterface;
};

/** @brief Endpoint descriptor. */
struct PBL_PACKED pbl_usb_desc_ep {
  /** Length, 7. */
  uint8_t bLength;
  /** @ref PBL_USB_DESC_ENDPOINT. */
  uint8_t bDescriptorType;
  /** Address: the number, with the direction in bit 7. */
  uint8_t bEndpointAddress;
  /** Transfer type, in bits 1:0. */
  uint8_t bmAttributes;
  /** Maximum packet size. */
  uint16_t wMaxPacketSize;
  /** Polling interval. */
  uint8_t bInterval;
};

/** @brief BOS descriptor, which device capability descriptors follow. */
struct PBL_PACKED pbl_usb_desc_bos {
  /** Length, 5. */
  uint8_t bLength;
  /** @ref PBL_USB_DESC_BOS. */
  uint8_t bDescriptorType;
  /** Length of the BOS and the capabilities that follow it. */
  uint16_t wTotalLength;
  /** Number of capabilities. */
  uint8_t bNumDeviceCaps;
};

/** @brief USB 2.0 extension capability. */
struct PBL_PACKED pbl_usb_desc_usb20_ext {
  /** Length, 7. */
  uint8_t bLength;
  /** @ref PBL_USB_DESC_DEVICE_CAP. */
  uint8_t bDescriptorType;
  /** @ref PBL_USB_CAP_USB20_EXT. */
  uint8_t bDevCapabilityType;
  /** Attributes, as link power management support. */
  uint32_t bmAttributes;
};

/** @brief Platform capability, which data of the platform follows. */
struct PBL_PACKED pbl_usb_desc_platform {
  /** Length, with the data of the platform. */
  uint8_t bLength;
  /** @ref PBL_USB_DESC_DEVICE_CAP. */
  uint8_t bDescriptorType;
  /** @ref PBL_USB_CAP_PLATFORM. */
  uint8_t bDevCapabilityType;
  /** Reserved, 0. */
  uint8_t bReserved;
  /** UUID of the platform, as the bytes of its little-endian form. */
  uint8_t PlatformCapabilityUUID[16];
};

/** @cond INTERNAL_HIDDEN */
static_assert(sizeof(struct pbl_usb_setup) == 8U);
static_assert(sizeof(struct pbl_usb_desc_header) == 2U);
static_assert(sizeof(struct pbl_usb_desc_device) == 18U);
static_assert(sizeof(struct pbl_usb_desc_qualifier) == 10U);
static_assert(sizeof(struct pbl_usb_desc_config) == 9U);
static_assert(sizeof(struct pbl_usb_desc_iad) == 8U);
static_assert(sizeof(struct pbl_usb_desc_iface) == 9U);
static_assert(sizeof(struct pbl_usb_desc_ep) == 7U);
static_assert(sizeof(struct pbl_usb_desc_bos) == 5U);
static_assert(sizeof(struct pbl_usb_desc_usb20_ext) == 7U);
static_assert(sizeof(struct pbl_usb_desc_platform) == 20U);
/** @endcond */

/** @} */
