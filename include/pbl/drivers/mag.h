/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pbl/device.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/mutex.h>

/**
 * @defgroup drivers_mag Magnetometer
 * @ingroup drivers
 * @brief Magnetometer class.
 *
 * A magnetometer is a struct pbl_mag device. While sampling, the driver reports each new sample to
 * the listener a consumer registered with pbl_mag_set_listener(), the compass service in practice.
 * pbl_mag_peek() reads the most recent sample instead, for consumers that poll.
 *
 * Every call into the driver, and every report out of it, runs with the device lock held, a
 * recursive mutex. The listener runs with it held too, so it may call back into the device.
 *
 * @code{.c}
 * static void prv_sample(const struct pbl_mag *mag, const MagData *sample, void *user_data) {
 *   ...
 * }
 *
 * static const struct pbl_mag_listener s_listener = {.sample = prv_sample};
 *
 * pbl_mag_set_listener(MAG, &s_listener);
 * pbl_mag_set_sampling_interval(MAG, 200000, nullptr);
 * @endcode
 * @{
 */

/** @brief Magnetometer sample, in milligauss, in the watch frame. */
typedef struct PBL_PACKED {
  /** Magnetic field along the X axis. */
  int16_t x;
  /** Magnetic field along the Y axis. */
  int16_t y;
  /** Magnetic field along the Z axis. */
  int16_t z;
} MagData;

struct pbl_mag;

/** @brief Magnetometer listener. */
struct pbl_mag_listener {
  /** Optional. New sample. */
  void (*sample)(const struct pbl_mag *mag, const MagData *sample, void *user_data);
  /** Data passed to the callbacks. */
  void *user_data;
};

/** @brief Magnetometer driver operations, called with the device lock held. */
struct pbl_mag_ops {
  /** Bring the hardware up. Returns 0, -ENODEV if the part does not answer, or a negative errno. */
  int (*init)(const struct pbl_mag *mag);
  /** Optional. Set whether the sensor is mounted rotated by 180 degrees. */
  int (*set_rotated)(const struct pbl_mag *mag, bool rotated);
  /** Set the sampling interval, 0 to stop sampling, returning the one used in @p actual_us. */
  int (*set_sampling_interval)(const struct pbl_mag *mag, uint32_t interval_us,
                               uint32_t *actual_us);
  /** Read the most recent sample. Returns -ENODATA when not sampling. */
  int (*peek)(const struct pbl_mag *mag, MagData *sample);
};

/** @brief Magnetometer runtime state. */
struct pbl_mag_state {
  /** Device lock. */
  struct pbl_mutex lock;
  /** Listener, or NULL. */
  const struct pbl_mag_listener *listener;
  /** Sampling interval in microseconds, 0 when not sampling. */
  uint32_t sampling_interval_us;
};

/** @brief Magnetometer. */
struct pbl_mag {
  /** Device. */
  struct pbl_device dev;
  /** Driver operations. */
  const struct pbl_mag_ops *ops;
  /** Runtime state. */
  struct pbl_mag_state *state;
};

/** @brief The board's magnetometer. */
extern const struct pbl_mag *const MAG;

/**
 * @brief Define the class state of a magnetometer.
 *
 * @param sym Symbol of the instance.
 */
#define PBL_MAG_STATE_DEFINE(sym) \
  PBL_DEVICE_STATE_DEFINE(sym);   \
  static struct pbl_mag_state sym##_mag_state = {.lock = PBL_MUTEX_INITIALIZER}

/**
 * @brief Initialize a struct pbl_mag.
 *
 * @param sym Symbol of the instance, as passed to PBL_MAG_STATE_DEFINE().
 * @param _name Device name.
 * @param _parent Parent device.
 * @param _ops Driver operations.
 */
#define PBL_MAG_INIT(sym, _name, _parent, _ops)                             \
  {                                                                         \
    .dev = PBL_DEVICE_INIT(sym, _name, pbl_mag_dev_init, _parent, nullptr), \
    .ops = (_ops),                                                          \
    .state = &sym##_mag_state,                                              \
  }

/** @cond INTERNAL_HIDDEN */
int pbl_mag_dev_init(const struct pbl_device *dev);
/** @endcond */

/**
 * @brief Take the device lock.
 *
 * @param mag Magnetometer.
 */
void pbl_mag_lock(const struct pbl_mag *mag);

/**
 * @brief Release the device lock.
 *
 * @param mag Magnetometer.
 */
void pbl_mag_unlock(const struct pbl_mag *mag);

/**
 * @brief Set the listener.
 *
 * @param mag Magnetometer.
 * @param listener Listener, or NULL to remove it.
 * @return 0.
 */
int pbl_mag_set_listener(const struct pbl_mag *mag, const struct pbl_mag_listener *listener);

/**
 * @brief Set whether the watch is mounted rotated by 180 degrees.
 *
 * When rotated, the X and Y axes of subsequent samples are inverted.
 *
 * @param mag Magnetometer.
 * @param rotated true if rotated.
 * @return 0, -ENODEV if the magnetometer is not ready, or a negative errno.
 */
int pbl_mag_set_rotated(const struct pbl_mag *mag, bool rotated);

/**
 * @brief Set the sampling interval.
 *
 * @param mag Magnetometer.
 * @param interval_us Requested interval in microseconds, 0 to stop sampling.
 * @param[out] actual_us Interval in use, or NULL.
 * @return 0, -ENODEV if the magnetometer is not ready, or a negative errno.
 */
int pbl_mag_set_sampling_interval(const struct pbl_mag *mag, uint32_t interval_us,
                                  uint32_t *actual_us);

/**
 * @brief Get the sampling interval.
 *
 * @param mag Magnetometer.
 * @return Interval in microseconds, 0 when not sampling or not ready.
 */
uint32_t pbl_mag_get_sampling_interval(const struct pbl_mag *mag);

/**
 * @brief Read the most recent sample.
 *
 * @param mag Magnetometer.
 * @param[out] sample Sample.
 * @return 0, -ENODEV if the magnetometer is not ready, -ENODATA when not sampling, or a negative
 *         errno.
 */
int pbl_mag_peek(const struct pbl_mag *mag, MagData *sample);

/**
 * @brief Report a new sample to the listener.
 *
 * For drivers. Must be called with the device lock held.
 *
 * @param mag Magnetometer.
 * @param sample Sample.
 */
void pbl_mag_report_sample(const struct pbl_mag *mag, const MagData *sample);

/** @} */
