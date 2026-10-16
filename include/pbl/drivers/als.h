/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_als Ambient light sensor
 * @ingroup drivers
 * @brief Ambient light sensor (ALS) class.
 *
 * An ambient light sensor is a struct pbl_als device. Readings are raw counts between 0 and
 * #AMBIENT_LIGHT_LEVEL_MAX; a calibrated instance converts them to lux with pbl_als_to_lux().
 *
 * Sampling is controlled by two reference counts kept by the class: a consumer that will read the
 * sensor soon primes it, and code that would disturb the reading (e.g. backlight bleed-through)
 * suspends it. The driver is told whenever the resulting state changes, and samples while primed
 * and not suspended.
 *
 * Every call into the driver runs with the device lock held, a recursive mutex.
 *
 * @code{.c}
 * uint32_t level;
 *
 * pbl_als_prime(ALS);
 * ...
 * if (pbl_als_read(ALS, &level) == 0) {
 *   uint32_t lux = pbl_als_to_lux(ALS, level);
 * }
 * pbl_als_release(ALS);
 * @endcode
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#ifndef CONFIG_AMBIENT_LIGHT_BITS
// Fallback for builds without Kconfig, e.g. unit tests.
#define CONFIG_AMBIENT_LIGHT_BITS 12
#endif
/** @endcond */

/** @brief Upper bound of the raw light level scale (@c CONFIG_AMBIENT_LIGHT_BITS bits). */
static const uint32_t AMBIENT_LIGHT_LEVEL_MAX = (1U << CONFIG_AMBIENT_LIGHT_BITS);

struct pbl_als;

/**
 * @brief Raw count to lux calibration: lux = (raw - offset) * num / den.
 *
 * Covers the sensor and the optical path in front of it, so it belongs to the board.
 */
struct pbl_als_scale {
  /** Dark offset, in raw counts. */
  uint32_t offset;
  /** Numerator. */
  uint32_t num;
  /** Denominator. */
  uint32_t den;
};

/** @brief Ambient light sensor driver operations, called with the device lock held. */
struct pbl_als_ops {
  /** Bring the hardware up. Returns 0, -ENODEV if the part does not answer, or a negative errno. */
  int (*init)(const struct pbl_als *als);
  /** Read the light level, in raw counts. */
  int (*read)(const struct pbl_als *als, uint32_t *level);
  /**
   * Optional. Apply the sampling state: @p active while primed, @p sampling while primed and not
   * suspended.
   */
  int (*set_state)(const struct pbl_als *als, bool active, bool sampling);
};

/** @brief Ambient light sensor runtime state. */
struct pbl_als_state {
  /** Device lock. */
  struct pbl_mutex lock;
  /** Prime references. */
  uint16_t prime_count;
  /** Suspend references. */
  uint16_t suspend_count;
};

/** @brief Ambient light sensor. */
struct pbl_als {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_als_ops *ops;
  /** Runtime state. */
  struct pbl_als_state *state;
  /** Lux calibration, or NULL if uncalibrated. */
  const struct pbl_als_scale *scale;
};

/** @brief The board's ambient light sensor. */
extern const struct pbl_als *const ALS;

/**
 * @brief Define the class state of an ambient light sensor.
 *
 * @param sym Symbol of the instance.
 */
#define PBL_ALS_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);   \
  static struct pbl_als_state sym##_als_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Initialize a struct pbl_als.
 *
 * @param sym Symbol of the instance, as passed to PBL_ALS_STATE_DEFINE().
 * @param _name Device name.
 * @param _parent Parent device.
 * @param _ops Driver operations.
 * @param _scale Lux calibration, or NULL.
 */
#define PBL_ALS_INIT(sym, _name, _parent, _ops, _scale)                     \
  {                                                                         \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_als_dev_init, _parent, nullptr), \
    .ops = (_ops),                                                          \
    .state = &sym##_als_state,                                              \
    .scale = (_scale),                                                      \
  }

/** @cond INTERNAL_HIDDEN */
int pbl_als_dev_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Read the light level.
 *
 * @param als Ambient light sensor.
 * @param[out] level Raw light level between 0 and #AMBIENT_LIGHT_LEVEL_MAX.
 * @return 0, -ENODEV if the sensor is not ready, or a negative errno.
 */
int pbl_als_read(const struct pbl_als *als, uint32_t *level);

/**
 * @brief Announce that readings will be wanted soon.
 *
 * Reference counted; balance with pbl_als_release(). Lets drivers that sample continuously start
 * ahead of the first read. Does nothing if the sensor is not ready.
 *
 * @param als Ambient light sensor.
 */
void pbl_als_prime(const struct pbl_als *als);

/**
 * @brief Drop a reference taken with pbl_als_prime().
 *
 * @param als Ambient light sensor.
 */
void pbl_als_release(const struct pbl_als *als);

/**
 * @brief Stop sampling until the matching pbl_als_resume().
 *
 * Reference counted. Used while something disturbs the sensor, e.g. backlight bleed-through. Does
 * nothing if the sensor is not ready.
 *
 * @param als Ambient light sensor.
 */
void pbl_als_suspend(const struct pbl_als *als);

/**
 * @brief Drop a reference taken with pbl_als_suspend().
 *
 * @param als Ambient light sensor.
 */
void pbl_als_resume(const struct pbl_als *als);

/**
 * @brief Check whether the sensor is calibrated to lux.
 *
 * @param als Ambient light sensor.
 * @return true if pbl_als_to_lux() converts to lux.
 */
bool pbl_als_has_lux(const struct pbl_als *als);

/**
 * @brief Convert a light level to lux.
 *
 * On uncalibrated sensors the level is returned unchanged, so callers can use the result
 * unconditionally.
 *
 * @param als Ambient light sensor.
 * @param level Light level, after screen compensation if any.
 * @return Light level in lux, or @p level if uncalibrated.
 */
uint32_t pbl_als_to_lux(const struct pbl_als *als, uint32_t level);

/** @} */
