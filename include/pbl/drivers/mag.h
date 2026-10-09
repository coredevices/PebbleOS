/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/kernel/compiler.h>

/**
 * @defgroup drivers_mag Magnetometer
 * @ingroup drivers
 * @brief 3-axis magnetometer driver interface.
 *
 * The hardware is reference counted: it is powered while at least one user holds it through
 * mag_use() or mag_start_sampling(), and turned off when the last user calls mag_release().
 *
 * @code{.c}
 * MagData data;
 *
 * mag_start_sampling();
 * if (mag_read_data(&data) == MagReadSuccess) {
 *   // use data.x, data.y, data.z
 * }
 * mag_release();
 * @endcode
 * @{
 */

/** @brief 3-axis magnetometer sample, in milligauss, in the watch frame. */
typedef struct PBL_PACKED {
  /** Magnetic field along the X axis. */
  int16_t x;
  /** Magnetic field along the Y axis. */
  int16_t y;
  /** Magnetic field along the Z axis. */
  int16_t z;
} MagData;

/** @brief Result of mag_read_data(). */
typedef enum {
  /** A new sample was read. */
  MagReadSuccess = 0,
  /** The sample was overwritten while being read. */
  MagReadClobbered = -1,
  /** No sample is ready, or the bus transfer failed. */
  MagReadCommunicationFail = -2,
  /** The magnetometer is not sampling. */
  MagReadMagOff = -3,
  /** No magnetometer is present. */
  MagReadNoMag = -4,
} MagReadStatus;

/** @brief Magnetometer sampling rate. */
typedef enum {
  /** 20 Hz. */
  MagSampleRate20Hz,
  /** 5 Hz. */
  MagSampleRate5Hz
} MagSampleRate;

/**
 * @brief Initialize the magnetometer.
 *
 * Called once at startup, before any other function in this API.
 */
void mag_init(void);

/**
 * @brief Take a reference on the magnetometer hardware.
 *
 * Must be matched with a call to mag_release().
 */
void mag_use(void);

/**
 * @brief Take a reference on the magnetometer and start sampling at 5 Hz.
 *
 * Must be matched with a call to mag_release().
 */
void mag_start_sampling(void);

/**
 * @brief Drop a reference on the magnetometer hardware.
 *
 * The hardware is turned off when the last reference is dropped.
 */
void mag_release(void);

/**
 * @brief Read the latest sample.
 *
 * The caller must hold a reference and the magnetometer must be sampling.
 *
 * @param[out] data Sample.
 * @return Read status, @ref MagReadSuccess when @p data was filled.
 */
MagReadStatus mag_read_data(MagData *data);

/**
 * @brief Change the sampling rate.
 *
 * Does nothing and succeeds when no reference is held.
 *
 * @param rate New sampling rate.
 * @return true on success.
 */
bool mag_change_sample_rate(MagSampleRate rate);

/** @} */
