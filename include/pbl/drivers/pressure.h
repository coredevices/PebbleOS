/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_pressure Pressure sensor
 * @ingroup drivers
 * @brief Barometric pressure sensor class.
 *
 * A pressure sensor is a struct pbl_pressure device. Each read runs one conversion; the sensor
 * sleeps in between. Every call into the driver runs with the device lock held, a recursive mutex.
 *
 * @code{.c}
 * uint32_t pa;
 *
 * if (pbl_pressure_read(PRESSURE, &pa) == 0) {
 *   // pa is in pascals
 * }
 * @endcode
 * @{
 */

struct pbl_pressure;

/** @brief Pressure sensor driver operations, called with the device lock held. */
struct pbl_pressure_ops {
  /** Bring the hardware up. Returns 0, -ENODEV if the part does not answer, or a negative errno. */
  int (*init)(const struct pbl_pressure *pressure);
  /** Run one conversion: pressure in pascals. */
  int (*read)(const struct pbl_pressure *pressure, uint32_t *pa);
};

/** @brief Pressure sensor runtime state. */
struct pbl_pressure_state {
  /** Device lock. */
  struct pbl_mutex lock;
};

/** @brief Pressure sensor. */
struct pbl_pressure {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_pressure_ops *ops;
  /** Runtime state. */
  struct pbl_pressure_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_pressure_class;
int pbl_pressure_dev_init(const struct pbl_device *dev);
/** @endcond */

/** @brief The board's barometric pressure sensor. */
extern const struct pbl_pressure *const PRESSURE;

/**
 * @brief Define the class state of a pressure sensor.
 *
 * @param sym Symbol of the instance.
 */
#define PBL_PRESSURE_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);        \
  static struct pbl_pressure_state sym##_pressure_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Initialize a struct pbl_pressure.
 *
 * @param sym Symbol of the instance, as passed to PBL_PRESSURE_STATE_DEFINE().
 * @param _name Device name.
 * @param _parent Parent device.
 * @param _ops Driver operations.
 */
#define PBL_PRESSURE_INIT(sym, _name, _parent, _ops)                                               \
  {                                                                                                \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_pressure_dev_init, _parent, &pbl_pressure_class), \
    .ops = (_ops),                                                                                 \
    .state = &sym##_pressure_state,                                                                \
  }

/**
 * @brief Measure the pressure.
 *
 * Runs one conversion, which takes tens of milliseconds and sleeps the caller.
 *
 * @param pressure Pressure sensor.
 * @param[out] pa Pressure in pascals.
 * @return 0, -ENODEV if the sensor is not ready, -ETIMEDOUT if the conversion timed out, or a
 *         negative errno.
 */
int pbl_pressure_read(const struct pbl_pressure *pressure, uint32_t *pa);

/** @} */
