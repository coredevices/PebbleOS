/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/kernel/section.h>
#include <pbl/usb/usb.h>

/**
 * @defgroup usbd USB device
 * @ingroup subsys
 * @brief The device side of USB, on the board's USB device controller, its UDC role.
 *
 * The device has one configuration, made of the functions defined with
 * PBL_USBD_FUNCTION_DEFINE(), in the order of their names. Each function gives its interface and
 * endpoint descriptors, with its interface numbers and endpoint addresses: the functions of a
 * device do not share any. The stack sets the maximum packet size of their bulk endpoints for the
 * bus speed, enables their endpoints when the host configures the device, and hands them the
 * class and vendor requests to their interfaces and the end of the transfers on their endpoints.
 * Everything the stack calls runs from the controller's interrupt.
 *
 * An interface can have alternate settings: its interface descriptor is followed by others with the
 * same interface number and a higher bAlternateSetting, each with its own endpoints. Two settings
 * may use the same endpoint addresses. Configuring the device selects setting 0 of every
 * interface; SET_INTERFACE then disables the endpoints of the current setting, enables those of
 * the new one and calls @ref pbl_usbd_function.set_alt.
 *
 * A function names its strings with its own indices, from 1, in the iInterface field of its
 * interface descriptors and the iFunction field of its interface association descriptor: index n
 * is @ref pbl_usbd_function.strings[n - 1]. The stack gives them indices of the device when it
 * sends the configuration descriptor. Strings in class-specific descriptors are not mapped.
 *
 * With @c CONFIG_USB_DEVICE_MS_OS_20 or @c CONFIG_USB_DEVICE_WEBUSB, the device is USB 2.1 and
 * has a BOS descriptor, with their platform capabilities, and the stack answers their vendor
 * requests to the device.
 * @{
 */

/** @brief A function of the device: a class, on interfaces and endpoints of its own. */
struct pbl_usbd_function {
  /**
   * Interface and endpoint descriptors, class-specific ones included, one after the other: as a
   * packed structure of @ref pbl_usb_desc_iface, @ref pbl_usb_desc_ep and the others.
   */
  const void *desc;
  /** Length of @ref desc, in bytes. */
  uint16_t desc_len;
  /** Optional. Strings of the descriptors, ASCII, by their index minus 1. */
  const char *const *strings;
  /** Number of @ref strings. */
  uint8_t num_strings;
  /** First interface. */
  uint8_t first_iface;
  /** Number of interfaces. */
  uint8_t num_ifaces;
  /**
   * Optional. The host configured the device, its endpoints now enabled, or left that
   * configuration, at a reset or a disconnection.
   */
  void (*configured)(bool on);
  /**
   * Optional. The host selected alternate setting @p alt of interface @p iface, the endpoints of
   * the previous setting now disabled, their transfers ended with @c -ECONNABORTED, and those of
   * @p alt enabled. Not called when the host configures the device.
   */
  void (*set_alt)(uint8_t iface, uint8_t alt);
  /**
   * Optional. A class or vendor request to an interface of the function. For an IN request, point
   * @p data at what to send, @p len bytes; for an OUT one, at room for @p len bytes, which then
   * arrive through @ref control_out. Returns 0, or a negative errno for the request to stall.
   */
  int (*class_request)(const struct pbl_usb_setup *setup, uint8_t **data, uint16_t *len);
  /** Optional. The data of an OUT class or vendor request, the request done. */
  void (*control_out)(const struct pbl_usb_setup *setup, const uint8_t *data, uint16_t len);
  /** Optional. A transfer on an endpoint of the function ended: bytes moved, or a negative errno.
   */
  void (*xfer_done)(uint8_t ep, int ret);
  /**
   * Optional, with @c CONFIG_USB_DEVICE_MS_OS_20. Device interface GUID of the function, as
   * "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}": Windows binds the function to WinUSB, and
   * applications find it by this GUID.
   */
  const char *winusb_guid;
};

/**
 * @brief Define a function of the device.
 *
 * @param name Name, which also orders the functions.
 */
#define PBL_USBD_FUNCTION_DEFINE(name)                                                 \
  static const struct pbl_usbd_function pbl_usbd_function_##name PBL_ITERABLE_SECTION( \
      const struct pbl_usbd_function, usbd_functions, #name)

/**
 * @brief Connect the device to the bus, once: the controller first, if it is not up yet.
 *
 * @return 0, or a negative errno, as @c -ENODEV when the controller failed.
 */
int pbl_usbd_enable(void);

/**
 * @brief Maximum packet size of the bulk endpoints, at the speed the bus runs.
 *
 * @return 64 at full speed, 512 at high speed.
 */
uint16_t pbl_usbd_bulk_mps(void);

/** @} */
