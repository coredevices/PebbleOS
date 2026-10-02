/* SPDX-FileCopyrightText: 2025 Matthew Wardrop */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

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
static const uint8_t MMC5603NJ_STATUS1_OTP_READ_DONE_MASK = 0x10;
/** @brief Sensor is saturated. */
static const uint8_t MMC5603NJ_STATUS1_SAT_SENSOR_MASK = 0x20;
/** @brief Magnetic measurement done. */
static const uint8_t MMC5603NJ_STATUS1_MEAS_M_DONE_MASK = 0x40;
/** @brief Temperature measurement done. */
static const uint8_t MMC5603NJ_STATUS1_MEAS_T_DONE_MASK = 0x80;

/** @} */

/**
 * @name CTRL0 bits
 * @{
 */

/** @brief Take a single magnetic field measurement. */
static const uint8_t MMC5603NJ_CTRL0_TAKE_MEAS_M = 0x01;
/** @brief Take a single temperature measurement. */
static const uint8_t MMC5603NJ_CTRL0_TAKE_MEAS_T = 0x02;
/** @brief Perform a single set operation. */
static const uint8_t MMC5603NJ_CTRL0_DO_SET = 0x08;
/** @brief Perform a single reset operation. */
static const uint8_t MMC5603NJ_CTRL0_DO_RESET = 0x10;
/** @brief Enable automatic set/reset. */
static const uint8_t MMC5603NJ_CTRL0_AUTO_SR_EN = 0x20;
/** @brief Perform a single self-test. */
static const uint8_t MMC5603NJ_CTRL0_AUTO_ST_EN = 0x40;
/**
 * @brief Compute the measurement period for the ODR.
 *
 * Must be set before continuous-mode measurements are started.
 */
static const uint8_t MMC5603NJ_CTRL0_CMM_FREQ_EN = 0x80;

/** @} */

/**
 * @name CTRL1 bits
 *
 * The bandwidth sets the measurement length; longer measurements have lower noise.
 * @{
 */

/** @brief Bandwidth 0: 6.6 ms measurement. */
static const uint8_t MMC5603NJ_CTRL1_BANDWIDTH_6ms6 = 0x00;
/** @brief Bandwidth 1: 3.5 ms measurement. */
static const uint8_t MMC5603NJ_CTRL1_BANDWIDTH_3ms5 = 0x01;
/** @brief Bandwidth 2: 2.0 ms measurement. */
static const uint8_t MMC5603NJ_CTRL1_BANDWIDTH_2ms = 0x02;
/** @brief Bandwidth 3: 1.2 ms measurement. */
static const uint8_t MMC5603NJ_CTRL1_BANDWIDTH_1ms2 = 0x03;
/** @brief Disable the X channel, shortening the measurement. */
static const uint8_t MMC5603NJ_CTRL1_X_INHIBIT = 0x04;
/** @brief Disable the Y channel, shortening the measurement. */
static const uint8_t MMC5603NJ_CTRL1_Y_INHIBIT = 0x08;
/** @brief Disable the Z channel, shortening the measurement. */
static const uint8_t MMC5603NJ_CTRL1_Z_INHIBIT = 0x10;
/**
 * @brief Drive a DC current through the self-test coil, offsetting the measured field.
 *
 * Used to check whether the sensor is saturated.
 */
static const uint8_t MMC5603NJ_CTRL1_ST_ENP = 0x20;
/** @brief As @ref MMC5603NJ_CTRL1_ST_ENP, in the opposite direction. */
static const uint8_t MMC5603NJ_CTRL1_ST_ENM = 0x40;
/** @brief Software reset: clear all registers and reread OTP. */
static const uint8_t MMC5603NJ_CTRL1_SW_RESET = 0x80;

/** @} */

/**
 * @name CTRL2 bits
 *
 * AUTOSET_PRD_n performs an automatic set/reset every n measurements.
 * @{
 */

/** @brief Automatic set/reset every measurement. */
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_1 = 0x00;
/** @brief Automatic set/reset every 25 measurements. */
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_25 = 0x01;
/** @brief Automatic set/reset every 75 measurements. */
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_75 = 0x02;
/** @brief Automatic set/reset every 100 measurements. */
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_100 = 0x03;
/** @brief Automatic set/reset every 250 measurements. */
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_250 = 0x04;
/** @brief Automatic set/reset every 500 measurements. */
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_500 = 0x05;
/** @brief Automatic set/reset every 1000 measurements. */
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_1000 = 0x06;
/** @brief Automatic set/reset every 2000 measurements. */
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_2000 = 0x07;
/** @brief Enable periodic automatic set (recommended). */
static const uint8_t MMC5603NJ_CTRL2_PRD_SET_EN = 0x08;
/**
 * @brief Enable continuous measurement mode.
 *
 * Requires the ODR and @ref MMC5603NJ_CTRL0_CMM_FREQ_EN to be set.
 */
static const uint8_t MMC5603NJ_CTRL2_CMM_EN = 0x10;
/** @brief High power mode, allowing an ODR up to 1000 Hz. */
static const uint8_t MMC5603NJ_CTRL2_HPOWER = 0x80;

/** @} */

/** @} */
