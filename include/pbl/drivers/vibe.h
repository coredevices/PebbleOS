/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_vibe Vibration motor
 * @ingroup drivers
 * @brief Vibration motor class.
 *
 * A vibration motor is a struct pbl_vibe device. Every call into the driver runs with the device
 * lock held, a recursive mutex.
 *
 * @code{.c}
 * pbl_vibe_start(VIBE, VIBE_STRENGTH_MAX);
 * // ...
 * pbl_vibe_stop(VIBE);
 * @endcode
 * @{
 */

/** @brief Full power. */
#define VIBE_STRENGTH_MAX 100
/** @brief Full reverse, for drivers that can brake. */
#define VIBE_STRENGTH_MIN -100
/** @brief Stopped. */
#define VIBE_STRENGTH_OFF 0

/** @brief Calibration value of a driver that keeps nothing to persist. */
#define PBL_VIBE_CALIBRATION_NONE 0xFFU

struct pbl_vibe;

/** @brief Vibration motor driver operations, called with the device lock held. */
struct pbl_vibe_ops {
  /** Bring the hardware up. Returns 0, -ENODEV if the part does not answer, or a negative errno. */
  int (*init)(const struct pbl_vibe *vibe);
  /** Drive the motor at @p strength, starting it if stopped. */
  int (*start)(const struct pbl_vibe *vibe, int8_t strength);
  /** Stop the motor. */
  int (*stop)(const struct pbl_vibe *vibe);
  /** Optional. Calibrate, setting @p cali to the value to persist. */
  int (*calibrate)(const struct pbl_vibe *vibe, uint8_t *cali);
  /** Optional. Apply a value set by calibrate. */
  int (*set_calibration)(const struct pbl_vibe *vibe, uint8_t cali);
};

/** @brief Vibration motor runtime state. */
struct pbl_vibe_state {
  /** Device lock. */
  struct pbl_mutex lock;
};

/** @brief Vibration motor. */
struct pbl_vibe {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_vibe_ops *ops;
  /** Runtime state. */
  struct pbl_vibe_state *state;
  /** Strength that brakes the motor, @ref VIBE_STRENGTH_OFF if the driver cannot brake. */
  int8_t braking_strength;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_device_class pbl_vibe_class;
int pbl_vibe_dev_init(const struct pbl_device *dev);
/** @endcond */

/** @brief The board's vibration motor. */
extern const struct pbl_vibe *const VIBE;

/**
 * @brief Define the class state of a vibration motor.
 *
 * @param sym Symbol of the instance.
 */
#define PBL_VIBE_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);    \
  static struct pbl_vibe_state sym##_vibe_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Initialize a struct pbl_vibe.
 *
 * @param sym Symbol of the instance, as passed to PBL_VIBE_STATE_DEFINE().
 * @param _name Device name.
 * @param _parent Parent device.
 * @param _ops Driver operations.
 * @param _braking_strength Strength that brakes the motor, or @ref VIBE_STRENGTH_OFF.
 */
#define PBL_VIBE_INIT(sym, _name, _parent, _ops, _braking_strength)                              \
  {                                                                                              \
    .dev =                                                                                       \
        PBL_DEVICE_INIT_CLASS(sym, _name, pbl_vibe_dev_init, _parent, nullptr, &pbl_vibe_class), \
    .ops = (_ops),                                                                               \
    .state = &sym##_vibe_state,                                                                  \
    .braking_strength = (_braking_strength),                                                     \
  }

/**
 * @brief Drive the motor, starting it if stopped.
 *
 * Drivers without reverse drive use the absolute value of @p strength.
 *
 * @param vibe Vibration motor.
 * @param strength Strength from @ref VIBE_STRENGTH_MIN to @ref VIBE_STRENGTH_MAX.
 * @return 0, -ENODEV if the motor is not ready, or a negative errno.
 */
int pbl_vibe_start(const struct pbl_vibe *vibe, int8_t strength);

/**
 * @brief Stop the motor.
 *
 * @param vibe Vibration motor.
 * @return 0, -ENODEV if the motor is not ready, or a negative errno.
 */
int pbl_vibe_stop(const struct pbl_vibe *vibe);

/**
 * @brief Get the strength that brakes the motor to a stop.
 *
 * @param vibe Vibration motor.
 * @return Braking strength, @ref VIBE_STRENGTH_OFF if the driver does not brake this way.
 */
int8_t pbl_vibe_get_braking_strength(const struct pbl_vibe *vibe);

/**
 * @brief Calibrate the motor driver.
 *
 * @param vibe Vibration motor.
 * @param[out] cali Value to persist and later pass to pbl_vibe_set_calibration(), or
 *                  @ref PBL_VIBE_CALIBRATION_NONE.
 * @return 0, -ENOTSUP if the driver cannot calibrate, -ENODEV if the motor is not ready, or a
 *         negative errno.
 */
int pbl_vibe_calibrate(const struct pbl_vibe *vibe, uint8_t *cali);

/**
 * @brief Apply a calibration value returned by pbl_vibe_calibrate().
 *
 * @param vibe Vibration motor.
 * @param cali Calibration value.
 * @return 0, -ENOTSUP if the driver cannot calibrate, -ENODEV if the motor is not ready, or a
 *         negative errno.
 */
int pbl_vibe_set_calibration(const struct pbl_vibe *vibe, uint8_t cali);

/** @} */
