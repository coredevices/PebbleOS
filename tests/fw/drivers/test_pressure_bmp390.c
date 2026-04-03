/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pebble_tasks.h"
#include "stubs_prompt.h"
#include "stubs_syscall_internal.h"

#include "drivers/pressure.h"
#include "fakes/fake_i2c.h"

#include <string.h>

// Stub for delay_us — no-op in tests
void delay_us(uint32_t us) { (void)us; }
void delay_init(void) { }

// BMP390 register addresses (must match driver)
#define REG_CHIP_ID       0x00
#define REG_STATUS        0x03
#define REG_DATA          0x04
#define REG_INT_STATUS    0x11
#define REG_PWR_CTRL      0x1B
#define REG_OSR           0x1C
#define REG_ODR           0x1D
#define REG_CONFIG        0x1F
#define REG_CALIB_DATA    0x31
#define REG_CMD           0x7E

#define CHIP_ID_VALUE     0x60
#define CALIB_DATA_LEN    21

// PWR_CTRL bit definitions
#define PRESS_EN          (1 << 0)
#define TEMP_EN           (1 << 1)
#define MODE_FORCED       (1 << 4)
#define MODE_NORMAL       (3 << 4)

// IIR filter coefficients (CONFIG register bits [3:1])
#define IIR_COEFF_0       (0x00 << 1)
#define IIR_COEFF_7       (0x03 << 1)
#define IIR_COEFF_15      (0x04 << 1)
#define IIR_COEFF_31      (0x05 << 1)

// INT_STATUS bits
#define DRDY_BIT          (1 << 3)

// Known calibration data from a real BMP390 — produces verifiable output
static const uint8_t s_test_calib_data[CALIB_DATA_LEN] = {
  // par_t1 (uint16): 27356 = 0x6ACC
  0xCC, 0x6A,
  // par_t2 (uint16): 18789 = 0x4965
  0x65, 0x49,
  // par_t3 (int8): -7
  0xF9,
  // par_p1 (int16): 217 = 0x00D9
  0xD9, 0x00,
  // par_p2 (int16): -163 = 0xFF5D
  0x5D, 0xFF,
  // par_p3 (int8): 14
  0x0E,
  // par_p4 (int8): 0
  0x00,
  // par_p5 (uint16): 25352 = 0x6308
  0x08, 0x63,
  // par_p6 (uint16): 29536 = 0x7360
  0x60, 0x73,
  // par_p7 (int8): -1
  0xFF,
  // par_p8 (int8): -7
  0xF9,
  // par_p9 (int16): 13000 = 0x32C8
  0xC8, 0x32,
  // par_p10 (int8): 29
  0x1D,
  // par_p11 (int8): 0
  0x00,
};

// Hook for simulating BMP390 behavior on register writes
static void prv_bmp390_write_hook(uint8_t reg, uint8_t value, void *context) {
  if (reg == REG_CMD && value == 0xB6) {
    // Soft reset: set status.cmd_rdy (bit 4) so driver sees device ready
    fake_i2c_set_register(REG_STATUS, 0x10);
  }

  if (reg == REG_PWR_CTRL && (value & MODE_FORCED)) {
    // Forced mode: set DRDY after "measurement"
    fake_i2c_set_register(REG_INT_STATUS, DRDY_BIT);
  }
}

// Hook that clears normal mode bits (simulates the hardware bug)
static void prv_normal_mode_bug_hook(uint8_t reg, uint8_t value, void *context) {
  prv_bmp390_write_hook(reg, value, context);

  if (reg == REG_PWR_CTRL) {
    uint8_t cleared = value & ~MODE_NORMAL;
    fake_i2c_set_register(REG_PWR_CTRL, cleared);
  }
}

static void prv_setup_bmp390_registers(void) {
  fake_i2c_init();
  fake_i2c_set_register(REG_CHIP_ID, CHIP_ID_VALUE);
  fake_i2c_set_register(REG_STATUS, 0x10);  // cmd_rdy
  fake_i2c_set_register_block(REG_CALIB_DATA, s_test_calib_data, CALIB_DATA_LEN);
  fake_i2c_set_write_hook(prv_bmp390_write_hook, NULL);
}

// Set up raw sensor data registers (pressure + temperature, 6 bytes at 0x04)
static void prv_set_sensor_data(uint32_t raw_pressure, uint32_t raw_temperature) {
  uint8_t data[6];
  data[0] = (raw_pressure >>  0) & 0xFF;
  data[1] = (raw_pressure >>  8) & 0xFF;
  data[2] = (raw_pressure >> 16) & 0xFF;
  data[3] = (raw_temperature >>  0) & 0xFF;
  data[4] = (raw_temperature >>  8) & 0xFF;
  data[5] = (raw_temperature >> 16) & 0xFF;
  fake_i2c_set_register_block(REG_DATA, data, 6);
}

// ---- Setup / Teardown ----

void test_pressure_bmp390__initialize(void) {
  prv_setup_bmp390_registers();
}

void test_pressure_bmp390__cleanup(void) {
  fake_i2c_set_register(REG_CHIP_ID, 0x00);
  fake_i2c_set_fail(false);
  pressure_init();
}

// ---- Init Tests ----

void test_pressure_bmp390__init_success(void) {
  pressure_init();
  // After init, driver should have written OSR register
  cl_assert(fake_i2c_was_written(REG_OSR, 0x03));   // 8x pressure, 1x temp
  // IIR defaults to off (FILTER_NONE)
  cl_assert(fake_i2c_was_written(REG_CONFIG, IIR_COEFF_0));
  // PWR_CTRL should enable sensors without mode bits (sleep)
  cl_assert(fake_i2c_was_written(REG_PWR_CTRL, PRESS_EN | TEMP_EN));
}

void test_pressure_bmp390__init_bad_chip_id(void) {
  fake_i2c_set_register(REG_CHIP_ID, 0xAA);
  pressure_init();
  cl_assert(!fake_i2c_was_written(REG_OSR, 0x03));
}

void test_pressure_bmp390__init_i2c_failure(void) {
  fake_i2c_set_fail(true);
  pressure_init();
  int32_t pressure, temp;
  cl_assert(!pressure_read(&pressure, &temp));
}

// ---- Forced Mode Read Tests ----

void test_pressure_bmp390__read_triggers_forced_mode(void) {
  pressure_init();
  prv_set_sensor_data(6892135, 8360755);

  int32_t pressure_pa, temp_centideg;
  cl_assert(pressure_read(&pressure_pa, &temp_centideg));

  cl_assert(fake_i2c_was_written(REG_PWR_CTRL,
                                 PRESS_EN | TEMP_EN | MODE_FORCED));

  cl_assert(pressure_pa > 80000);
  cl_assert(pressure_pa < 120000);
  cl_assert(temp_centideg > -4000);
  cl_assert(temp_centideg < 8500);
}

void test_pressure_bmp390__read_not_initialized(void) {
  int32_t pressure, temp;
  cl_assert(!pressure_read(&pressure, &temp));
}

void test_pressure_bmp390__read_null_params(void) {
  pressure_init();
  prv_set_sensor_data(6892135, 8360755);

  cl_assert(pressure_read(NULL, NULL));

  int32_t pressure;
  cl_assert(pressure_read(&pressure, NULL));
  cl_assert(pressure > 80000);

  int32_t temp;
  cl_assert(pressure_read(NULL, &temp));
}

// ---- ODR Configuration Tests ----

void test_pressure_bmp390__set_odr_25hz(void) {
  pressure_init();
  cl_assert(pressure_set_odr(PRESSURE_ODR_25HZ));

  cl_assert_equal_i(fake_i2c_last_written(REG_ODR), 0x03);    // BMP390_ODR_25HZ
  cl_assert_equal_i(fake_i2c_last_written(REG_OSR), 0x01);    // OSR_P_2X | OSR_T_1X
  // Default filter mode is NONE — IIR should be off
  cl_assert_equal_i(fake_i2c_last_written(REG_CONFIG), IIR_COEFF_0);
}

void test_pressure_bmp390__set_odr_50hz(void) {
  pressure_init();
  cl_assert(pressure_set_odr(PRESSURE_ODR_50HZ));

  cl_assert_equal_i(fake_i2c_last_written(REG_ODR), 0x02);    // BMP390_ODR_50HZ
  cl_assert_equal_i(fake_i2c_last_written(REG_OSR), 0x00);    // OSR_P_1X | OSR_T_1X
  cl_assert_equal_i(fake_i2c_last_written(REG_CONFIG), IIR_COEFF_0);
}

void test_pressure_bmp390__set_odr_1hz(void) {
  pressure_init();
  cl_assert(pressure_set_odr(PRESSURE_ODR_1HZ));

  cl_assert_equal_i(fake_i2c_last_written(REG_ODR), 0x07);    // BMP390_ODR_1P5625HZ
  cl_assert_equal_i(fake_i2c_last_written(REG_OSR), 0x03);    // OSR_P_8X | OSR_T_1X
  cl_assert_equal_i(fake_i2c_last_written(REG_CONFIG), IIR_COEFF_0);
}

void test_pressure_bmp390__set_odr_5hz(void) {
  pressure_init();
  cl_assert(pressure_set_odr(PRESSURE_ODR_5HZ));

  cl_assert_equal_i(fake_i2c_last_written(REG_ODR), 0x05);    // BMP390_ODR_6P25HZ
  cl_assert_equal_i(fake_i2c_last_written(REG_OSR), 0x03);    // OSR_P_8X | OSR_T_1X
  cl_assert_equal_i(fake_i2c_last_written(REG_CONFIG), IIR_COEFF_0);
}

void test_pressure_bmp390__set_odr_10hz(void) {
  pressure_init();
  cl_assert(pressure_set_odr(PRESSURE_ODR_10HZ));

  cl_assert_equal_i(fake_i2c_last_written(REG_ODR), 0x04);    // BMP390_ODR_12P5HZ
  cl_assert_equal_i(fake_i2c_last_written(REG_OSR), 0x02);    // OSR_P_4X | OSR_T_1X
  cl_assert_equal_i(fake_i2c_last_written(REG_CONFIG), IIR_COEFF_0);
}

void test_pressure_bmp390__set_odr_enables_normal_mode(void) {
  pressure_init();
  cl_assert(pressure_set_odr(PRESSURE_ODR_25HZ));

  uint8_t last_pwr = fake_i2c_last_written(REG_PWR_CTRL);
  cl_assert_equal_i(last_pwr, PRESS_EN | TEMP_EN | MODE_NORMAL);
}

void test_pressure_bmp390__set_odr_not_initialized(void) {
  cl_assert(!pressure_set_odr(PRESSURE_ODR_25HZ));
}

// ---- Filter Mode Tests ----

void test_pressure_bmp390__filter_mode_default_none(void) {
  pressure_init();
  // Init should write IIR off
  cl_assert(fake_i2c_was_written(REG_CONFIG, IIR_COEFF_0));
}

void test_pressure_bmp390__filter_mode_smooth_applies_iir(void) {
  pressure_init();
  cl_assert(pressure_set_filter_mode(PRESSURE_FILTER_SMOOTH));

  // Should immediately write IIR coeff to CONFIG
  cl_assert_equal_i(fake_i2c_last_written(REG_CONFIG), IIR_COEFF_7);
}

void test_pressure_bmp390__filter_mode_none_clears_iir(void) {
  pressure_init();
  pressure_set_filter_mode(PRESSURE_FILTER_SMOOTH);
  cl_assert(pressure_set_filter_mode(PRESSURE_FILTER_NONE));

  cl_assert_equal_i(fake_i2c_last_written(REG_CONFIG), IIR_COEFF_0);
}

void test_pressure_bmp390__filter_smooth_odr_25hz(void) {
  pressure_init();
  pressure_set_filter_mode(PRESSURE_FILTER_SMOOTH);
  cl_assert(pressure_set_odr(PRESSURE_ODR_25HZ));

  cl_assert_equal_i(fake_i2c_last_written(REG_CONFIG), IIR_COEFF_15);
}

void test_pressure_bmp390__filter_smooth_odr_50hz(void) {
  pressure_init();
  pressure_set_filter_mode(PRESSURE_FILTER_SMOOTH);
  cl_assert(pressure_set_odr(PRESSURE_ODR_50HZ));

  cl_assert_equal_i(fake_i2c_last_written(REG_CONFIG), IIR_COEFF_31);
}

void test_pressure_bmp390__filter_smooth_odr_1hz(void) {
  pressure_init();
  pressure_set_filter_mode(PRESSURE_FILTER_SMOOTH);
  cl_assert(pressure_set_odr(PRESSURE_ODR_1HZ));

  cl_assert_equal_i(fake_i2c_last_written(REG_CONFIG), IIR_COEFF_7);
}

void test_pressure_bmp390__filter_none_odr_25hz(void) {
  // Verify that FILTER_NONE overrides ODR-specific IIR
  pressure_init();
  pressure_set_filter_mode(PRESSURE_FILTER_NONE);
  cl_assert(pressure_set_odr(PRESSURE_ODR_25HZ));

  cl_assert_equal_i(fake_i2c_last_written(REG_CONFIG), IIR_COEFF_0);
}

void test_pressure_bmp390__filter_mode_not_initialized(void) {
  cl_assert(!pressure_set_filter_mode(PRESSURE_FILTER_SMOOTH));
}

// ---- Normal Mode Bug Simulation ----

void test_pressure_bmp390__normal_mode_bits_written(void) {
  fake_i2c_set_write_hook(prv_normal_mode_bug_hook, NULL);
  pressure_init();

  cl_assert(pressure_set_odr(PRESSURE_ODR_25HZ));

  cl_assert(fake_i2c_was_written(REG_PWR_CTRL,
                                 PRESS_EN | TEMP_EN | MODE_NORMAL));

  uint8_t readback = fake_i2c_get_register(REG_PWR_CTRL);
  cl_assert_equal_i(readback & MODE_NORMAL, 0);
}

// ---- Altitude Calculation Tests ----

void test_pressure_bmp390__altitude_cm_sea_level(void) {
  int32_t alt = pressure_get_altitude_cm(101325, 101325);
  cl_assert_equal_i(alt, 0);
}

void test_pressure_bmp390__altitude_cm_above_sea_level(void) {
  int32_t alt = pressure_get_altitude_cm(100500, 101325);
  cl_assert(alt > 6000);
  cl_assert(alt < 8000);
}

void test_pressure_bmp390__altitude_cm_below_sea_level(void) {
  int32_t alt = pressure_get_altitude_cm(102000, 101325);
  cl_assert(alt < 0);
}

void test_pressure_bmp390__altitude_cm_zero_reference(void) {
  int32_t alt = pressure_get_altitude_cm(101325, 0);
  cl_assert_equal_i(alt, 0);
}

void test_pressure_bmp390__altitude_full_sea_level(void) {
  int32_t alt = pressure_get_altitude_full_cm(101325, 101325);
  cl_assert_equal_i(alt, 0);
}

void test_pressure_bmp390__altitude_full_skydive_exit(void) {
  // ~4000m (~13000ft): pressure approximately 61640 Pa
  int32_t alt = pressure_get_altitude_full_cm(61640, 101325);
  cl_assert(alt > 380000);
  cl_assert(alt < 420000);
}

void test_pressure_bmp390__altitude_full_negative(void) {
  int32_t alt = pressure_get_altitude_full_cm(103000, 101325);
  cl_assert(alt < 0);
}

void test_pressure_bmp390__altitude_full_zero_input(void) {
  cl_assert_equal_i(pressure_get_altitude_full_cm(0, 101325), 0);
  cl_assert_equal_i(pressure_get_altitude_full_cm(101325, 0), 0);
}

// ---- Compensation Math Consistency ----

void test_pressure_bmp390__read_consistency(void) {
  pressure_init();
  prv_set_sensor_data(6892135, 8360755);

  int32_t p1, t1, p2, t2;
  cl_assert(pressure_read(&p1, &t1));
  cl_assert(pressure_read(&p2, &t2));
  cl_assert_equal_i(p1, p2);
  cl_assert_equal_i(t1, t2);
}
