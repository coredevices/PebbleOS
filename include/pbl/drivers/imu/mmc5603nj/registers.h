/* SPDX-FileCopyrightText: 2025 Matthew Wardrop */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/**
 * @defgroup drivers_imu_mmc5603nj_registers MMC5603NJ registers
 * @ingroup drivers_imu_mmc5603nj
 * @brief MMC5603NJ register map, from the MEMSIC datasheet (Rev. B).
 *
 * Datasheet:
 * https://www.memsic.com/Public/Uploads/uploadfile/files/20220119/MMC5603NJDatasheetRev.B.pdf
 *
 * Multi-byte values are assumed little endian on the host.
 *
 * The output data rate (ODR) is set in Hz, 1 to 255. With @ref MMC5603NJ_CTRL2_HPOWER set,
 * ODR=255 selects 1000 Hz. The achievable ODR is limited by the bandwidth and by automatic
 * set/reset (recommended):
 *
 * | Bandwidth     | Max ODR                                            |
 * |---------------|----------------------------------------------------|
 * | 0 (6.6 ms)    | 75 Hz with automatic set/reset, else 150 Hz        |
 * | 1 (3.5 ms)    | 150 Hz with automatic set/reset, else 255 Hz       |
 * | 2 (2.0 ms)    | 255 Hz                                             |
 * | 3 (1.2 ms)    | 255 Hz, or 1000 Hz in high power mode              |
 * @{
 */

/**
 * @name Register addresses
 * @{
 */

/** @brief X output, bits 19:12. */
static const uint8_t MMC5603NJ_REG_XOUT0 = 0x00;
/** @brief X output, bits 11:4. */
static const uint8_t MMC5603NJ_REG_XOUT1 = 0x01;
/** @brief Y output, bits 19:12. */
static const uint8_t MMC5603NJ_REG_YOUT0 = 0x02;
/** @brief Y output, bits 11:4. */
static const uint8_t MMC5603NJ_REG_YOUT1 = 0x03;
/** @brief Z output, bits 19:12. */
static const uint8_t MMC5603NJ_REG_ZOUT0 = 0x04;
/** @brief Z output, bits 11:4. */
static const uint8_t MMC5603NJ_REG_ZOUT1 = 0x05;
/** @brief X output, bits 3:0. */
static const uint8_t MMC5603NJ_REG_XOUT2 = 0x06;
/** @brief Y output, bits 3:0. */
static const uint8_t MMC5603NJ_REG_YOUT2 = 0x07;
/** @brief Z output, bits 3:0. */
static const uint8_t MMC5603NJ_REG_ZOUT2 = 0x08;
/** @brief Temperature output. */
static const uint8_t MMC5603NJ_REG_TOUT = 0x09;
/** @brief Device status. */
static const uint8_t MMC5603NJ_REG_STATUS1 = 0x18;
/** @brief Output data rate. */
static const uint8_t MMC5603NJ_REG_ODR = 0x1A;
/** @brief Control register 0. */
static const uint8_t MMC5603NJ_REG_CTRL0 = 0x1B;
/** @brief Control register 1. */
static const uint8_t MMC5603NJ_REG_CTRL1 = 0x1C;
/** @brief Control register 2. */
static const uint8_t MMC5603NJ_REG_CTRL2 = 0x1D;
/** @brief X-axis self-test threshold. */
static const uint8_t MMC5603NJ_REG_ST_X_TH = 0x1E;
/** @brief Y-axis self-test threshold. */
static const uint8_t MMC5603NJ_REG_ST_Y_TH = 0x1F;
/** @brief Z-axis self-test threshold. */
static const uint8_t MMC5603NJ_REG_ST_Z_TH = 0x20;
/** @brief X-axis self-test set value. */
static const uint8_t MMC5603NJ_REG_ST_X = 0x27;
/** @brief Y-axis self-test set value. */
static const uint8_t MMC5603NJ_REG_ST_Y = 0x28;
/** @brief Z-axis self-test set value. */
static const uint8_t MMC5603NJ_REG_ST_Z = 0x29;
/** @brief Product ID. */
static const uint8_t MMC5603NJ_REG_WHO_AM_I = 0x39;

/** @} */

/**
 * @name Device constants
 * @{
 */

/** @brief Expected @ref MMC5603NJ_REG_WHO_AM_I value. */
static const uint8_t MMC5603NJ_WHO_AM_I_VALUE = 0x10;
/** @brief Delay in milliseconds required after a software reset. */
static const uint8_t MMC5603NJ_SW_RESET_DELAY_MS = 20;
/** @brief Delay in milliseconds required after a set or reset operation. */
static const uint8_t MMC5603NJ_SET_DELAY_MS = 1;

/** @} */

/**
 * @name STATUS1 bits
 * @{
 */

/** @brief OTP memory has been read. */
#define MMC5603NJ_STATUS1_OTP_READ_DONE_MASK PBL_BIT(4)
/** @brief Sensor is saturated. */
#define MMC5603NJ_STATUS1_SAT_SENSOR_MASK PBL_BIT(5)
/** @brief Magnetic measurement done. */
#define MMC5603NJ_STATUS1_MEAS_M_DONE_MASK PBL_BIT(6)
/** @brief Temperature measurement done. */
#define MMC5603NJ_STATUS1_MEAS_T_DONE_MASK PBL_BIT(7)

/** @} */

/**
 * @name CTRL0 bits
 * @{
 */

/** @brief Take a single magnetic field measurement. */
#define MMC5603NJ_CTRL0_TAKE_MEAS_M PBL_BIT(0)
/** @brief Take a single temperature measurement. */
#define MMC5603NJ_CTRL0_TAKE_MEAS_T PBL_BIT(1)
/** @brief Perform a single set operation. */
#define MMC5603NJ_CTRL0_DO_SET PBL_BIT(3)
/** @brief Perform a single reset operation. */
#define MMC5603NJ_CTRL0_DO_RESET PBL_BIT(4)
/** @brief Enable automatic set/reset. */
#define MMC5603NJ_CTRL0_AUTO_SR_EN PBL_BIT(5)
/** @brief Perform a single self-test. */
#define MMC5603NJ_CTRL0_AUTO_ST_EN PBL_BIT(6)
/**
 * @brief Compute the measurement period for the ODR.
 *
 * Must be set before continuous-mode measurements are started.
 */
#define MMC5603NJ_CTRL0_CMM_FREQ_EN PBL_BIT(7)

/** @} */

/**
 * @name CTRL1 bits
 *
 * The bandwidth sets the measurement length; longer measurements have lower noise.
 * @{
 */

/** @brief Bandwidth field. */
#define MMC5603NJ_CTRL1_BANDWIDTH_MASK PBL_GENMASK(1, 0)
/** @brief Bandwidth 0: 6.6 ms measurement. */
#define MMC5603NJ_CTRL1_BANDWIDTH_6ms6 0U
/** @brief Bandwidth 1: 3.5 ms measurement. */
#define MMC5603NJ_CTRL1_BANDWIDTH_3ms5 1U
/** @brief Bandwidth 2: 2.0 ms measurement. */
#define MMC5603NJ_CTRL1_BANDWIDTH_2ms 2U
/** @brief Bandwidth 3: 1.2 ms measurement. */
#define MMC5603NJ_CTRL1_BANDWIDTH_1ms2 3U
/** @brief Disable the X channel, shortening the measurement. */
#define MMC5603NJ_CTRL1_X_INHIBIT PBL_BIT(2)
/** @brief Disable the Y channel, shortening the measurement. */
#define MMC5603NJ_CTRL1_Y_INHIBIT PBL_BIT(3)
/** @brief Disable the Z channel, shortening the measurement. */
#define MMC5603NJ_CTRL1_Z_INHIBIT PBL_BIT(4)
/**
 * @brief Drive a DC current through the self-test coil, offsetting the measured field.
 *
 * Used to check whether the sensor is saturated.
 */
#define MMC5603NJ_CTRL1_ST_ENP PBL_BIT(5)
/** @brief As @ref MMC5603NJ_CTRL1_ST_ENP, in the opposite direction. */
#define MMC5603NJ_CTRL1_ST_ENM PBL_BIT(6)
/** @brief Software reset: clear all registers and reread OTP. */
#define MMC5603NJ_CTRL1_SW_RESET PBL_BIT(7)

/** @} */

/**
 * @name CTRL2 bits
 *
 * AUTOSET_PRD_n performs an automatic set/reset every n measurements.
 * @{
 */

/** @brief Automatic set/reset period field. */
#define MMC5603NJ_CTRL2_AUTOSET_PRD_MASK PBL_GENMASK(2, 0)
/** @brief Automatic set/reset every measurement. */
#define MMC5603NJ_CTRL2_AUTOSET_PRD_1 0U
/** @brief Automatic set/reset every 25 measurements. */
#define MMC5603NJ_CTRL2_AUTOSET_PRD_25 1U
/** @brief Automatic set/reset every 75 measurements. */
#define MMC5603NJ_CTRL2_AUTOSET_PRD_75 2U
/** @brief Automatic set/reset every 100 measurements. */
#define MMC5603NJ_CTRL2_AUTOSET_PRD_100 3U
/** @brief Automatic set/reset every 250 measurements. */
#define MMC5603NJ_CTRL2_AUTOSET_PRD_250 4U
/** @brief Automatic set/reset every 500 measurements. */
#define MMC5603NJ_CTRL2_AUTOSET_PRD_500 5U
/** @brief Automatic set/reset every 1000 measurements. */
#define MMC5603NJ_CTRL2_AUTOSET_PRD_1000 6U
/** @brief Automatic set/reset every 2000 measurements. */
#define MMC5603NJ_CTRL2_AUTOSET_PRD_2000 7U
/** @brief Enable periodic automatic set (recommended). */
#define MMC5603NJ_CTRL2_PRD_SET_EN PBL_BIT(3)
/**
 * @brief Enable continuous measurement mode.
 *
 * Requires the ODR and @ref MMC5603NJ_CTRL0_CMM_FREQ_EN to be set.
 */
#define MMC5603NJ_CTRL2_CMM_EN PBL_BIT(4)
/** @brief High power mode, allowing an ODR up to 1000 Hz. */
#define MMC5603NJ_CTRL2_HPOWER PBL_BIT(7)

/** @} */

/** @} */
