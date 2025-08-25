/*
 * Copyright 2025 Matthew Wardrop
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

# pragma once

// These definitions are drawn from the datasheet provided by MEMSIC:
// https://www.memsic.com/Public/Uploads/uploadfile/files/20220119/MMC5603NJDatasheetRev.B.pdf
// Note that it is assumed that the architectures we are building for are little endian

#define MMC5603NJ_REG_XOUT0                      0x00  // Xout[19:12]
#define MMC5603NJ_REG_XOUT1                      0x01  // Xout[11:4]
#define MMC5603NJ_REG_YOUT0                      0x02  // Yout[19:12]
#define MMC5603NJ_REG_YOUT1                      0x03  // Yout[11:4]
#define MMC5603NJ_REG_ZOUT0                      0x04  // Zout[19:12]
#define MMC5603NJ_REG_ZOUT1                      0x05  // Zout[11:4]
#define MMC5603NJ_REG_XOUT2                      0x06  // Xout[3:0]
#define MMC5603NJ_REG_YOUT2                      0x07  // Yout[3:0]
#define MMC5603NJ_REG_ZOUT2                      0x08  // Zout[3:0]
#define MMC5603NJ_REG_TOUT                       0x09  // Temperature output
#define MMC5603NJ_REG_STATUS1                    0x18  // Device status
#define MMC5603NJ_REG_ODR                        0x1A  // Output data rate
#define MMC5603NJ_REG_INTERNAL_CONTROL_0         0x1B  // Control register 0
#define MMC5603NJ_REG_INTERNAL_CONTROL_1         0x1C  // Control register 1
#define MMC5603NJ_REG_INTERNAL_CONTROL_2         0x1D  // Control register 2
#define MMC5603NJ_REG_ST_X_TH                    0x1E  // X-axis selftest threshold
#define MMC5603NJ_REG_ST_Y_TH                    0x1F  // Y-axis selftest threshold
#define MMC5603NJ_REG_ST_Z_TH                    0x20  // Z-axis selftest threshold
#define MMC5603NJ_REG_ST_X                       0x27  // X-axis selftest set value
#define MMC5603NJ_REG_ST_Y                       0x28  // Y-axis selftest set value
#define MMC5603NJ_REG_ST_Z                       0x29  // z-axis selftest set value
#define MMC5603NJ_REG_WHO_AM_I                   0x39  // Product ID
#define MMC5603NJ_WHO_AM_I_VALUE                 0x10  // Expected value for WHO_AM_I

typedef struct {
    uint8_t not_used       :4;  // Factory use only; reset value is 0
    bool otp_read_done     :1;  // Indicates succesful read of OTP memory.
    bool sat_sensor        :1;  // Indicates whether selftest is underway.
    bool meas_m_done       :1;  // Indicates measurement of magnetic field is ready.
    bool meas_t_done       :1;  // Indicates measurement of temperature is ready.
} mmc5603nj_device_status1_t;

typedef struct {
    bool take_meas_m       :1;  // Take a single measurement of the magnetic field.
    bool take_meas_t       :1;  // Take a single measurement of the temperature.
    bool not_used          :1;  // Factory use only, reset value is 0.
    bool do_set            :1;  // Perform a single set operation (allows large current to flow through sensor coils for 375ns).
    bool do_reset          :1;  // Peform a single reset operation (same as above).
    bool auto_sr_en        :1;  // Enable automatic set/reset (recommended).
    bool auto_st_en        :1;  // Perform a single selftest (make sure ST_{X,Y,Z}_TH are set first)
    bool cmm_freq_en       :1;  // Start the calculation of the measurement period for ODR.
                                // Should be set before continuous-mode measurements are started.
} mmc5603nj_internal_control_0_t;

typedef struct {
    uint8_t bandwidth      :2;  // Bandwidth selection (selects length of measurement, see mmc5603nj_bandwidth_t)
    bool x_inhibit         :1;  // Disable the X channel (reducing length of measurement)
    bool y_inhibit         :1;  // As above, but for Y.
    bool z_inhibit         :1;  // As above, but for Z.
    bool st_enp            :1;  // Bring a DC current through the self-test coil of sensor inducing offset to magnetic field.
                                // Checks whether the sensor has been saturated.
    bool st_enm            :1;  // Same as above, but in opposite direction.
    bool sw_reset          :1;  // Reset hardware clearing all registers and rereading OTP.
} mmc5603nj_internal_control_1_t;

typedef struct {
    uint8_t prd_set        :3;  // Length of period (in terms of no. measurements) before a set is issued automatically
                                    // (prd_set_en and auto_sr must be enabled); see mmc5603nj_autoset_prd_t.
    bool prd_set_en        :1;  // Enable automatic set (recommended).
    bool cmm_en            :1;  // Enable continuous measurement mode (ODR and cmm_freq_en must be set).
    uint8_t not_used       :2;  // Factory use only; reset value is 0.
    bool hpower            :1;  // High power mode (allowed for ODR up to 1000Hz).
} mmc5603nj_internal_control_2_t;

typedef enum {
    BANDWIDTH_6ms6 = 0,  // 6.6ms
    BANDWIDTH_3ms5 = 1,  // 3.5ms
    BANDWIDTH_2ms = 2,   // 2.0ms
    BANDWIDTH_1ms2 = 3   // 1.2ms
} mmc5603nj_bandwidth_t;

typedef enum {
    AUTOSET_PRD_1 = 0,
    AUTOSET_PRD_25 = 1,
    AUTOSET_PRD_75 = 2,
    AUTOSET_PRD_100 = 3,
    AUTOSET_PRD_250 = 4,
    AUTOSET_PRD_500 = 5,
    AUTOSET_PRD_1000 = 6,
    AUTOSET_PRD_2000 = 7
} mmc5603nj_autoset_prd_t;

#define MMC5603NJ_SW_RESET_DELAY_MS 20
#define MMC5603NJ_SET_DELAY_MS 1

/*
A note on ODRs

The ODR can be configured in the range of 1-255, with an increment of 1.

1000Hz can be achieved by setting hpower=1 in INTERNAL_CONTROL_2 and ODR=255; 
otherwise it is interpreted as Hz.

The target ODR may not be achievable if automatic set/reset is enabled (recommended),
or if the bandwidth is high. The actual ODR achieved is as follows:

BW=0 (6.6ms), Max ODR = 75Hz if auto set/reset enabled, else 150 Hz
BW=1 (3.5ms), Max ODR = 150Hz if auto set/reset enabled, else 255 Hz
BW=2 (2.0ms), Max ODR = 255Hz
BW=3 (1.2ms), Max ODR = 255Hz if hpower=0, else 1000Hz
*/
