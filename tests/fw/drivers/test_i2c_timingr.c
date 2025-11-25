/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "drivers/stm32f7/i2c_timingr.h"

#define MHZ_TO_HZ(val) ((val) * 1000000)
#define KHZ_TO_HZ(val) ((val) * 1000)


static void prv_check_result(uint32_t timingr, uint32_t expected_prescaler,
                             uint32_t expected_scl_low, uint32_t expected_scl_high,
                             uint32_t expected_scl_delay) {
  cl_assert_(timingr != I2C_TIMINGR_INVALID_VALUE, "timingr == I2C_TIMINGR_INVALID_VALUE");
  cl_assert_equal_i((timingr >> 28) + 1, expected_prescaler);
  cl_assert_equal_i((timingr & 0xFF) + 1, expected_scl_low);
  cl_assert_equal_i(((timingr >> 8) & 0xFF) + 1, expected_scl_high);
  cl_assert_equal_i(((timingr >> 20) & 0xF) + 1, expected_scl_delay);
}

void test_i2c_timingr__valid_no_prescaler_no_rise_fall_time(void) {
  // We'll use a base clock speed of 36Mhz and try to get to 400kHz I2C. We should be able to do
  // this with a prescaler of 1.
  //
  // 36MHz / 400kHz = 90 cycles => 90 - 6 sync cycles = 84 cycles to play with
  // minimum low = ceil(1300ns / (1 / 36MHz)) = 47 cycles
  // minimum high = ceil(600ns / (1 / 36MHz)) = 22 cycles
  // extra cycles = 15 => (7 low, 8 high) => SCLL of 54 and SCLH of 30
  //
  // SCLDEL = ceil((t_r 0 + t_SU 100ns) / (1 / 36MHz)) = 4 cycles
  prv_check_result(i2c_timingr_calculate(MHZ_TO_HZ(36), I2CBusMode_FastMode,
                                         KHZ_TO_HZ(400), 0, 0),
                   1, 54, 30, 4);
}

void test_i2c_timingr__valid_prescaler_no_rise_fall_time(void) {
  // We'll use a base clock speed of 360Mhz and try to get to 100kHz I2C. This requires a prescaler
  // of 8 which gets us down to a base clock speed of 45MHz.
  //
  // 45MHz / 100kHz = 450 cycles => 450 - ceil(6 / 8) sync cycles = 449 cycles to play with
  // minimum low = ceil(4700ns / (1 / 45MHz)) = 212 cycles
  // minimum high = ceil(4000ns / (1 / 45MHz)) = 181 cycles
  // extra cycles = 56 => (28 low, 28 high) => SCLL of 240 and SCLH of 209
  //
  // SCLDEL = ceil((t_r 0 + t_SU 250ns) / (1 / 45MHz)) = 12
  prv_check_result(i2c_timingr_calculate(MHZ_TO_HZ(360), I2CBusMode_Standard,
                                         KHZ_TO_HZ(100), 0, 0),
                   8, 240, 209, 12);
}

void test_i2c_timingr__valid_no_prescaler_rise_fall_time(void) {
  // We'll use a base clock speed of 20MHz and try to get to 100kHz I2C with fall and rise times of
  // 500ns each.
  //
  // 20MHz / 100kHz = 200 cycles => 200 - 6 sync cycles - (2 * 500ns / (1 / 20MHz)) = 174 cycles to
  // play with
  // minimum low = ceil(4700ns / (1 / 20MHz)) = 94 cycles
  // minimum high = ceil(4000ns / (1 / 20MHz)) = 80 cycles
  // extra cycles = 0 => (0 low, 0 high) => SCLL of 94 and SCLH of 80
  //
  // SCLDEL = ceil((t_r 500ns + t_SU 250ns) / (1 / 20MHz)) = 15
  prv_check_result(i2c_timingr_calculate(MHZ_TO_HZ(20), I2CBusMode_Standard,
                                         KHZ_TO_HZ(100), 500, 500),
                   1, 94, 80, 15);
}

void test_i2c_timingr__data_delay_requires_prescaler(void) {
  // We'll increase the rise time enough that the required SCLDEL will exceed the max value with
  // no prescaler, forcing the use of the prescaler even though the SCLL and SCLH values wouldn't
  // otherwise require it.
  //
  // With prescaler 1: SCLDEL = ceil((800ns + 250ns) / (1 / 20MHz)) = 21 > 16
  // With prescaler 2: base clock is 10MHz.
  // 10MHz / 100kHz = 100 cycles => 100 - 6 sync cycles - ((800ns + 200ns) / (1 / 10 MHz)) = 84
  // cycles to play with
  // minimum low = ceil(4700ns / (1 / 10MHz)) = 47 cycles
  // minimum high = ceil(4000ns / (1 / 10MHz)) = 40 cycles
  // extra cycles = 0 => (0 low, 0 high) => SCLL of 47 and SCLH of 80
  //
  // SCLDEL = ceil((t_r 800ns + t_SU 250ns) / (1 / 10MHz)) = 11
  prv_check_result(i2c_timingr_calculate(MHZ_TO_HZ(20), I2CBusMode_Standard,
                                         KHZ_TO_HZ(100), 800, 200),
                   2, 47, 40, 11);
}

void test_i2c_timingr__invalid_speed_too_high(void) {
  // We'll use a base clock speed of 1Mhz and try to get to 400KHz I2C, which won't be possible
  // because the sync cycles alone will make us way slower than 400kHz.
  cl_assert_equal_i(i2c_timingr_calculate(MHZ_TO_HZ(1), I2CBusMode_FastMode,
                                          KHZ_TO_HZ(400), 0, 0),
                    I2C_TIMINGR_INVALID_VALUE);
}

void test_i2c_timingr__invalid_speed_too_low(void) {
  // We'll use a base clock speed of 1600Mhz and try to get to 100KHz I2C, which won't be possible
  // because the max prescaler is 16, which still leaves us with 1000 clock periods which is too
  // many to fit.
  cl_assert_equal_i(i2c_timingr_calculate(MHZ_TO_HZ(1600), I2CBusMode_Standard,
                                          KHZ_TO_HZ(100), 0, 0),
                    I2C_TIMINGR_INVALID_VALUE);
}

void test_i2c_timingr__invalid_speed_too_high_for_mode(void) {
  // Try calculating timing for 400kHz in Standard mode, which is out of spec for that mode.
  cl_assert_equal_i(i2c_timingr_calculate(MHZ_TO_HZ(36), I2CBusMode_Standard,
                                          KHZ_TO_HZ(400), 0, 0),
                    I2C_TIMINGR_INVALID_VALUE);
}

void test_i2c_timingr__invalid_long_rise_fall(void) {
  // We'll use a base clock speed of 100Mhz and try to get to 100KHz I2C with very long (out of
  // spec) 5us rise and fall times which prevent us from hitting the target frequency.
  cl_assert_equal_i(i2c_timingr_calculate(MHZ_TO_HZ(100), I2CBusMode_Standard,
                                          KHZ_TO_HZ(100), 5000, 5000),
                    I2C_TIMINGR_INVALID_VALUE);
}
