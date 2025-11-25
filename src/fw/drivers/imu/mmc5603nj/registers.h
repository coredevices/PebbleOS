/* SPDX-FileCopyrightText: 2025 Matthew Wardrop */
/* SPDX-License-Identifier: Apache-2.0 */

# pragma once

// These definitions are drawn from the datasheet provided by MEMSIC:
// https://www.memsic.com/Public/Uploads/uploadfile/files/20220119/MMC5603NJDatasheetRev.B.pdf
// Note that it is assumed that the architectures we are building for are little endian

static const uint8_t MMC5603NJ_REG_XOUT0                     = 0x00;  // Xout[19:12]
static const uint8_t MMC5603NJ_REG_XOUT1                     = 0x01;  // Xout[11:4]
static const uint8_t MMC5603NJ_REG_YOUT0                     = 0x02;  // Yout[19:12]
static const uint8_t MMC5603NJ_REG_YOUT1                     = 0x03;  // Yout[11:4]
static const uint8_t MMC5603NJ_REG_ZOUT0                     = 0x04;  // Zout[19:12]
static const uint8_t MMC5603NJ_REG_ZOUT1                     = 0x05;  // Zout[11:4]
static const uint8_t MMC5603NJ_REG_XOUT2                     = 0x06;  // Xout[3:0]
static const uint8_t MMC5603NJ_REG_YOUT2                     = 0x07;  // Yout[3:0]
static const uint8_t MMC5603NJ_REG_ZOUT2                     = 0x08;  // Zout[3:0]
static const uint8_t MMC5603NJ_REG_TOUT                      = 0x09;  // Temperature output
static const uint8_t MMC5603NJ_REG_STATUS1                   = 0x18;  // Device status
static const uint8_t MMC5603NJ_REG_ODR                       = 0x1A;  // Output data rate
static const uint8_t MMC5603NJ_REG_CTRL0                     = 0x1B;  // Control register 0
static const uint8_t MMC5603NJ_REG_CTRL1                     = 0x1C;  // Control register 1
static const uint8_t MMC5603NJ_REG_CTRL2                     = 0x1D;  // Control register 2
static const uint8_t MMC5603NJ_REG_ST_X_TH                   = 0x1E;  // X-axis selftest threshold
static const uint8_t MMC5603NJ_REG_ST_Y_TH                   = 0x1F;  // Y-axis selftest threshold
static const uint8_t MMC5603NJ_REG_ST_Z_TH                   = 0x20;  // Z-axis selftest threshold
static const uint8_t MMC5603NJ_REG_ST_X                      = 0x27;  // X-axis selftest set value
static const uint8_t MMC5603NJ_REG_ST_Y                      = 0x28;  // Y-axis selftest set value
static const uint8_t MMC5603NJ_REG_ST_Z                      = 0x29;  // z-axis selftest set value
static const uint8_t MMC5603NJ_REG_WHO_AM_I                  = 0x39;  // Product ID

static const uint8_t MMC5603NJ_WHO_AM_I_VALUE                = 0x10;  // Expected value for WHO_AM_I
static const uint8_t MMC5603NJ_SW_RESET_DELAY_MS             = 20;    // Required 20 ms delay after a software reset
static const uint8_t MMC5603NJ_SET_DELAY_MS                  = 1;     // Required 1 ms delay after a (re)set operation

// STATUS 1 masks
static const uint8_t MMC5603NJ_STATUS1_OTP_READ_DONE_MASK    = 0x10; // Indicates whether the OTP memory has been read.
static const uint8_t MMC5603NJ_STATUS1_SAT_SENSOR_MASK       = 0x20; // Indicates whether the sensor is saturated.
static const uint8_t MMC5603NJ_STATUS1_MEAS_M_DONE_MASK      = 0x40; // Indicates whether the magnetic measurement is done.
static const uint8_t MMC5603NJ_STATUS1_MEAS_T_DONE_MASK      = 0x80; // Indicates whether the temperature measurement is done.

// CONTROL 0 bits
static const uint8_t MMC5603NJ_CTRL0_TAKE_MEAS_M             = 0x01;  // Take a single measurement of the magnetic field.
static const uint8_t MMC5603NJ_CTRL0_TAKE_MEAS_T             = 0x02;  // Take a single measurement of the temperature.
static const uint8_t MMC5603NJ_CTRL0_DO_SET                  = 0x08;  // Perform a single set operation.
static const uint8_t MMC5603NJ_CTRL0_DO_RESET                = 0x10;  // Perform a single reset operation.
static const uint8_t MMC5603NJ_CTRL0_AUTO_SR_EN              = 0x20;  // Enable automatic set/reset.
static const uint8_t MMC5603NJ_CTRL0_AUTO_ST_EN              = 0x40;  // Perform a single selftest.
static const uint8_t MMC5603NJ_CTRL0_CMM_FREQ_EN             = 0x80;  // Start the calculation of the measurement period for ODR.
                                                                    // Should be set before continuous-mode measurements are started.

// CONTROL 1 bits
// Bandwidth settings control length of measurement (longer measurements have lower noise)
static const uint8_t MMC5603NJ_CTRL1_BANDWIDTH_6ms6          = 0x00;  // 6.6ms
static const uint8_t MMC5603NJ_CTRL1_BANDWIDTH_3ms5          = 0x01;  // 3.5ms
static const uint8_t MMC5603NJ_CTRL1_BANDWIDTH_2ms           = 0x02;  // 2.0ms
static const uint8_t MMC5603NJ_CTRL1_BANDWIDTH_1ms2          = 0x03;  // 1.2ms
static const uint8_t MMC5603NJ_CTRL1_X_INHIBIT               = 0x04;  // Disable the X channel (reducing length of measurement)
static const uint8_t MMC5603NJ_CTRL1_Y_INHIBIT               = 0x08;  // As above, but for Y.
static const uint8_t MMC5603NJ_CTRL1_Z_INHIBIT               = 0x10;  // As above, but for Z.
static const uint8_t MMC5603NJ_CTRL1_ST_ENP                  = 0x20;  // Bring a DC current through the self-test coil of sensor inducing offset to magnetic field.
                                                                      // Checks whether the sensor has been saturated.
static const uint8_t MMC5603NJ_CTRL1_ST_ENM                  = 0x40;  // Same as above, but in opposite direction.
static const uint8_t MMC5603NJ_CTRL1_SW_RESET                = 0x80;  // Reset hardware clearing all registers and rereading OTP.


// CONTROL 2 bits
// AUTOSET_PRD determines how often automatic set/reset operations are performed
// i.e. MMC5603NJ_CTRL2_AUTOSET_PRD_<n> means resets will occur every <n> measurements
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_1           = 0x00;
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_25          = 0x01;
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_75          = 0x02;
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_100         = 0x03;
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_250         = 0x04;
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_500         = 0x05;
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_1000        = 0x06;
static const uint8_t MMC5603NJ_CTRL2_AUTOSET_PRD_2000        = 0x07;
static const uint8_t MMC5603NJ_CTRL2_PRD_SET_EN              = 0x08;  // Enable automatic set (recommended).
static const uint8_t MMC5603NJ_CTRL2_CMM_EN                  = 0x10;  // Enable continuous measurement mode (ODR and cmm_freq_en must be set).
static const uint8_t MMC5603NJ_CTRL2_HPOWER                  = 0x80;  // High power mode (allowed for ODR up to 1000Hz).

/*
A note on ODRs

The ODR can be configured in the range of 1-255, with an increment of 1.

1000Hz can be achieved by setting hpower=1 in CTRL2 and ODR=255; 
otherwise it is interpreted as Hz.

The target ODR may not be achievable if automatic set/reset is enabled (recommended),
or if the bandwidth is high. The actual ODR achieved is as follows:

BW=0 (6.6ms), Max ODR = 75Hz if auto set/reset enabled, else 150 Hz
BW=1 (3.5ms), Max ODR = 150Hz if auto set/reset enabled, else 255 Hz
BW=2 (2.0ms), Max ODR = 255Hz
BW=3 (1.2ms), Max ODR = 255Hz if hpower=0, else 1000Hz
*/
