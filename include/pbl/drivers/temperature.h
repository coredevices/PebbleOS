/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_temperature Temperature sensor
 * @ingroup drivers
 * @brief Temperature sensor class.
 *
 * A temperature sensor is a struct pbl_temp device. Every call into the driver runs with the
 * device lock held, a recursive mutex.
 *
 * @code{.c}
 * int32_t temp;
 *
 * if (pbl_temp_read(DIE_TEMP, &temp) == 0) {
 *   // temp is in millidegrees Celsius
 * }
 * @endcode
 * @{
 */

struct pbl_temp;

/** @brief Temperature sensor driver operations, called with the device lock held. */
struct pbl_temp_ops {
  /** Optional. Bring the hardware up. */
  int (*init)(const struct pbl_temp *temp);
  /** Read the temperature, in millidegrees Celsius. */
  int (*read)(const struct pbl_temp *temp, int32_t *millicelsius);
};

/** @brief Temperature sensor runtime state. */
struct pbl_temp_state {
  /** Device lock. */
  struct pbl_mutex lock;
};

/** @brief Temperature sensor. */
struct pbl_temp {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_temp_ops *ops;
  /** Runtime state. */
  struct pbl_temp_state *state;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_temp_class;
int pbl_temp_dev_init(const struct pbl_device *dev);
/** @endcond */

/** @brief The board's die temperature sensor. */
extern const struct pbl_temp *const DIE_TEMP;

/**
 * @brief Define the class state of a temperature sensor.
 *
 * @param sym Symbol of the instance.
 */
#define PBL_TEMP_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);    \
  static struct pbl_temp_state sym##_temp_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Initialize a struct pbl_temp.
 *
 * @param sym Symbol of the instance, as passed to PBL_TEMP_STATE_DEFINE().
 * @param _name Device name.
 * @param _parent Parent device, or NULL.
 * @param _deps Dependencies from PBL_DEVICE_DEPS(), or NULL.
 * @param _ops Driver operations.
 */
#define PBL_TEMP_INIT(sym, _name, _parent, _deps, _ops)                                           \
  {                                                                                               \
    .dev = PBL_DEVICE_INIT_CLASS(sym, _name, pbl_temp_dev_init, _parent, _deps, &pbl_temp_class), \
    .ops = (_ops),                                                                                \
    .state = &sym##_temp_state,                                                                   \
  }

/**
 * @brief Read the temperature.
 *
 * @warning The sensor may not be calibrated: do not rely on the reading as an accurate absolute
 * temperature.
 *
 * @param temp Temperature sensor.
 * @param[out] millicelsius Temperature in millidegrees Celsius.
 * @return 0, -ENODEV if the sensor is not ready, -ETIMEDOUT if the conversion timed out, or a
 *         negative errno.
 */
int pbl_temp_read(const struct pbl_temp *temp, int32_t *millicelsius);

/** @} */
