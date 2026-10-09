/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/bluetooth/types.h>

/**
 * @defgroup bluetooth_hrm_service Heart Rate Service
 * @ingroup bluetooth
 * @brief GATT Heart Rate Service, sharing the watch's heart rate with connected devices.
 *
 * Implemented by backends that select @c CONFIG_BT_HRM_SERVICE. The NimBLE backend does not.
 * @{
 */

/** @brief A heart rate measurement. */
struct pbl_bt_hrm_service_measurement {
  /** Heart rate in beats per minute. */
  uint16_t bpm;
  /** True if the watch is worn. */
  bool is_on_wrist;
};

/**
 * @brief Add or remove the Heart Rate Service from the GATT database.
 *
 * Connected devices are sent a Service Changed indication for the affected handle range.
 *
 * @param enable true to add the service, false to remove it.
 */
void pbl_bt_hrm_service_enable(bool enable);

/**
 * @brief Send a heart rate measurement to the subscribed devices.
 *
 * @param measurement The measurement.
 * @param permitted_devices Devices allowed to receive it.
 * @param num_permitted_devices Number of entries in @p permitted_devices.
 */
void pbl_bt_hrm_service_handle_measurement(const struct pbl_bt_hrm_service_measurement *measurement,
                                           const struct pbl_bt_device_internal *permitted_devices,
                                           size_t num_permitted_devices);

/**
 * @brief Called when a connected device (un)subscribes to the Heart Rate Measurement
 * characteristic.
 *
 * @param device The device.
 * @param is_subscribed true if the device is now subscribed.
 */
extern void pbl_bt_cb_hrm_service_update_subscription(const struct pbl_bt_device_internal *device,
                                                      bool is_subscribed);

/** @} */
