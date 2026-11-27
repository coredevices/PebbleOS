/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/clk.h>
#include <pbl/drivers/sensor.h>

/**
 * @defgroup drivers_sensor_temp_sf32lb SF32LB TSEN
 * @ingroup drivers_sensor
 * @brief SiFli SF32LB die temperature sensor (TSEN).
 * @{
 */

/** @brief SF32LB die temperature sensor. */
struct pbl_temp_sf32lb {
  /** Sensor. */
  struct pbl_sensor sensor;
  /** Registers. */
  uintptr_t base;
  /** Clock. */
  struct pbl_clk clk;
  /** HPSYS configuration registers, holding the analog bandgap enable. */
  uintptr_t cfg;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_sensor_ops pbl_temp_sf32lb_ops;
/** @endcond */

/**
 * @brief Define the temperature sensor.
 *
 * @param sym Symbol.
 * @param _name Device name.
 * @param _base Registers.
 * @param _clk Clock, from PBL_CLK().
 * @param _cfg HPSYS configuration registers.
 */
#define PBL_TEMP_SF32LB_DEFINE(sym, _name, _base, _clk, _cfg)                                      \
  PBL_SENSOR_STATE_DEFINE(sym);                                                                    \
  PBL_DEVICE_INSTANCE(sym, struct pbl_temp_sf32lb, struct pbl_sensor, sensor) {                    \
    .sensor = PBL_SENSOR_INIT(sym, _name, nullptr, &pbl_temp_sf32lb_ops, PBL_BIT(PBL_SENSOR_TEMP), \
                              0U, PBL_SENSOR_MOUNT_IDENTITY),                                      \
    .base = (_base), .clk = _clk, .cfg = (_cfg),                                                   \
  }

/** @} */
