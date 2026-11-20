/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/drivers/i2c.h>
#include <pbl/drivers/pressure.h>

/**
 * @defgroup drivers_pressure_bmp390 BMP390
 * @ingroup drivers_pressure
 * @brief Bosch BMP380/BMP390 @ref drivers_pressure driver (compatible bosch,bmp380).
 *
 * Runs one forced-mode conversion per read and leaves the sensor in sleep mode otherwise.
 *
 * @code{.c}
 * PBL_BMP390_DEFINE(s_bmp390, &s_i2c, 0x76);
 *
 * const struct pbl_pressure *const PRESSURE = &s_bmp390;
 * @endcode
 * @{
 */

/** @brief BMP390 trimming coefficients, as stored in its NVM. */
struct pbl_bmp390_calib {
  uint16_t par_t1;
  uint16_t par_t2;
  int8_t par_t3;
  int16_t par_p1;
  int16_t par_p2;
  int8_t par_p3;
  int8_t par_p4;
  uint16_t par_p5;
  uint16_t par_p6;
  int8_t par_p7;
  int8_t par_p8;
  int16_t par_p9;
  int8_t par_p10;
  int8_t par_p11;
};

/** @brief BMP390 pressure sensor. */
struct pbl_bmp390 {
  /** Pressure sensor, child of the I2C bus. */
  struct pbl_pressure pressure;
  /** Trimming coefficients, read at init. */
  struct pbl_bmp390_calib *calib;
  /** I2C device. */
  struct pbl_i2c_dev i2c;
};

/** @cond INTERNAL_HIDDEN */
extern const struct pbl_pressure_ops pbl_bmp390_ops;
/** @endcond */

/**
 * @brief Define a BMP390.
 *
 * @param sym Symbol of the instance, a struct pbl_pressure.
 * @param _bus I2C bus.
 * @param _addr 7-bit I2C address.
 */
#define PBL_BMP390_DEFINE(sym, _bus, _addr)                                      \
  PBL_PRESSURE_STATE_DEFINE(sym);                                                \
  static struct pbl_bmp390_calib sym##_bmp390_calib;                             \
  PBL_DEVICE_INSTANCE(sym, struct pbl_bmp390, struct pbl_pressure, pressure) {   \
    .pressure = PBL_PRESSURE_INIT(sym, "bmp390", &(_bus)->dev, &pbl_bmp390_ops), \
    .calib = &sym##_bmp390_calib, .i2c = PBL_I2C_DEV(_bus, _addr),               \
  }

/** @} */
