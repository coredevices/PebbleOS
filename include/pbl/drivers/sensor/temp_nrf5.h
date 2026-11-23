/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/sensor.h>

/**
 * @defgroup drivers_sensor_temp_nrf5 nRF5 TEMP
 * @ingroup drivers_sensor
 * @brief Nordic nRF5 die temperature sensor (TEMP).
 * @{
 */

/** @brief nRF5 die temperature sensor. */
struct pbl_temp_nrf5 {
  /** Sensor. */
  struct pbl_sensor sensor;
  /** Registers. */
  uintptr_t base;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_sensor_ops pbl_temp_nrf5_ops;
/** @endcond */

/**
 * @brief Define the temperature sensor.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _base Registers.
 */
#define PBL_TEMP_NRF5_DEFINE(sym, _name, _base)                                                  \
  PBL_SENSOR_STATE_DEFINE(sym);                                                                  \
  PBL_DEVICE_INSTANCE(sym, struct pbl_temp_nrf5, struct pbl_sensor, sensor) {                    \
    .sensor = PBL_SENSOR_INIT(sym, _name, nullptr, &pbl_temp_nrf5_ops, PBL_BIT(PBL_SENSOR_TEMP), \
                              0U, PBL_SENSOR_MOUNT_IDENTITY),                                    \
    .base = (_base),                                                                             \
  }

/** @} */
