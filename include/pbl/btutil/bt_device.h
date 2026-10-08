/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

#include <pbl/bluetooth/types.h>

/**
 * @defgroup btutil Bluetooth utilities
 * @ingroup lib
 * @brief Stack independent helpers for Bluetooth devices, UUIDs and pairing data
 * (@c lib/btutil).
 *
 * @code{.c}
 * struct pbl_bt_device dev = bt_device_init_with_address(addr, false);
 *
 * if (!bt_device_is_invalid(&dev)) {
 *   struct pbl_bt_addr a = bt_device_get_address(dev);
 *   PBL_LOG_DBG("Device " PBL_BT_ADDR_FMT, PBL_BT_ADDR_XPLODE(a));
 * }
 *
 * Uuid hrs = bt_uuid_expand_16bit(0x180D); // 0000180D-0000-1000-8000-00805F9B34FB
 * @endcode
 * @{
 */

/**
 * @brief Create an LE device from its address.
 *
 * @param address The address.
 * @param is_random true for a random address, false for the public address (BD_ADDR).
 * @return The device.
 */
struct pbl_bt_device bt_device_init_with_address(struct pbl_bt_addr address, bool is_random);

/**
 * @brief Get the address of a device.
 *
 * @param device The device.
 * @return The address.
 */
struct pbl_bt_addr bt_device_get_address(struct pbl_bt_device device);

/**
 * @brief Compare two addresses.
 *
 * @param addr1 First address, may be NULL.
 * @param addr2 Second address, may be NULL.
 * @return true if both are non-NULL and equal.
 */
bool bt_device_address_equal(const struct pbl_bt_addr *addr1, const struct pbl_bt_addr *addr2);

/**
 * @brief Check whether an address is invalid.
 *
 * @param addr The address, may be NULL.
 * @return true if @p addr is NULL or all zeros.
 */
bool bt_device_address_is_invalid(const struct pbl_bt_addr *addr);

/**
 * @brief Compare two devices: address, address type and transport.
 *
 * @param device1_int First device, may be NULL.
 * @param device2_int Second device, may be NULL.
 * @return true if both are non-NULL and refer to the same device.
 */
bool bt_device_internal_equal(const struct pbl_bt_device_internal *device1_int,
                              const struct pbl_bt_device_internal *device2_int);

/**
 * @brief Compare two devices, see bt_device_internal_equal().
 *
 * @param device1 First device, may be NULL.
 * @param device2 Second device, may be NULL.
 * @return true if both are non-NULL and refer to the same device.
 */
bool bt_device_equal(const struct pbl_bt_device *device1, const struct pbl_bt_device *device2);

/**
 * @brief Check whether a device is invalid, such as one returned by an API that found no
 * device.
 *
 * @param device The device.
 * @return true if @p device equals PBL_BT_DEVICE_INVALID.
 */
bool bt_device_is_invalid(const struct pbl_bt_device *device);

/** @} */
