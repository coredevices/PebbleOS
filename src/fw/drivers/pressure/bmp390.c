/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "drivers/pressure.h"
#include "drivers/i2c.h"
#include "console/prompt.h"
#include "os/mutex.h"
#include "system/logging.h"

#include "kernel/util/delay.h"

#include <inttypes.h>
#include <string.h>

// BMP390 Register Map
#define BMP390_REG_CHIP_ID        0x00
#define BMP390_REG_DATA           0x04  // 6 bytes: press[0:2], temp[0:2]
#define BMP390_REG_INT_STATUS     0x11
#define BMP390_REG_PWR_CTRL      0x1B
#define BMP390_REG_OSR           0x1C
#define BMP390_REG_ODR           0x1D
#define BMP390_REG_CONFIG        0x1F
#define BMP390_REG_CALIB_DATA    0x31  // 21 bytes of calibration coefficients
#define BMP390_REG_CMD           0x7E

#define BMP390_CHIP_ID_VALUE     0x60
#define BMP390_CALIB_DATA_LEN    21
#define BMP390_DATA_LEN          6

// PWR_CTRL bits
#define BMP390_PRESS_EN          (1 << 0)
#define BMP390_TEMP_EN           (1 << 1)
#define BMP390_MODE_NORMAL       (3 << 4)
#define BMP390_MODE_FORCED       (1 << 4)

// Oversampling settings (for OSR register)
// pressure osr[2:0], temperature osr[5:3]
#define BMP390_OSR_P_1X          0x00  // 1x pressure oversampling
#define BMP390_OSR_P_2X          0x01  // 2x pressure oversampling
#define BMP390_OSR_P_4X          0x02  // 4x pressure oversampling
#define BMP390_OSR_P_8X          0x03  // 8x pressure oversampling
#define BMP390_OSR_T_1X          (0x00 << 3)  // 1x temperature oversampling

// IIR filter coefficients (for CONFIG register, bits [3:1])
#define BMP390_IIR_COEFF_0       (0x00 << 1)  // Off
#define BMP390_IIR_COEFF_1       (0x01 << 1)
#define BMP390_IIR_COEFF_3       (0x02 << 1)
#define BMP390_IIR_COEFF_7       (0x03 << 1)
#define BMP390_IIR_COEFF_15      (0x04 << 1)
#define BMP390_IIR_COEFF_31      (0x05 << 1)

// ODR register values (from BMP390 datasheet Table 22)
#define BMP390_ODR_200HZ         0x00
#define BMP390_ODR_100HZ         0x01
#define BMP390_ODR_50HZ          0x02
#define BMP390_ODR_25HZ          0x03
#define BMP390_ODR_12P5HZ        0x04
#define BMP390_ODR_6P25HZ        0x05
#define BMP390_ODR_3P125HZ       0x06
#define BMP390_ODR_1P5625HZ      0x07
#define BMP390_ODR_0P78125HZ     0x08
#define BMP390_ODR_0P390HZ       0x09

// INT_STATUS bits
#define BMP390_DRDY              (1 << 3)

// Calibration coefficient storage
typedef struct {
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
  int64_t t_lin;  // linearized temperature for pressure compensation
} BMP390CalibData;

static BMP390CalibData s_calib;
static bool s_initialized;
static PebbleMutex *s_mutex;
static PressureFilterMode s_filter_mode;

// I2C helpers using the PebbleOS I2C API
static bool prv_read_register(uint8_t reg, uint8_t *result) {
  i2c_use(I2C_BMP390);
  bool rv = i2c_read_register(I2C_BMP390, reg, result);
  i2c_release(I2C_BMP390);
  return rv;
}

static bool prv_read_register_block(uint8_t reg, uint32_t len, uint8_t *buf) {
  i2c_use(I2C_BMP390);
  bool rv = i2c_read_register_block(I2C_BMP390, reg, len, buf);
  i2c_release(I2C_BMP390);
  return rv;
}

static bool prv_write_register(uint8_t reg, uint8_t value) {
  i2c_use(I2C_BMP390);
  bool rv = i2c_write_register(I2C_BMP390, reg, value);
  i2c_release(I2C_BMP390);
  return rv;
}

static void prv_parse_calib_data(const uint8_t *reg_data) {
  s_calib.par_t1 = (uint16_t)(reg_data[1] << 8 | reg_data[0]);
  s_calib.par_t2 = (uint16_t)(reg_data[3] << 8 | reg_data[2]);
  s_calib.par_t3 = (int8_t)reg_data[4];
  s_calib.par_p1 = (int16_t)(reg_data[6] << 8 | reg_data[5]);
  s_calib.par_p2 = (int16_t)(reg_data[8] << 8 | reg_data[7]);
  s_calib.par_p3 = (int8_t)reg_data[9];
  s_calib.par_p4 = (int8_t)reg_data[10];
  s_calib.par_p5 = (uint16_t)(reg_data[12] << 8 | reg_data[11]);
  s_calib.par_p6 = (uint16_t)(reg_data[14] << 8 | reg_data[13]);
  s_calib.par_p7 = (int8_t)reg_data[15];
  s_calib.par_p8 = (int8_t)reg_data[16];
  s_calib.par_p9 = (int16_t)(reg_data[18] << 8 | reg_data[17]);
  s_calib.par_p10 = (int8_t)reg_data[19];
  s_calib.par_p11 = (int8_t)reg_data[20];
}

// Integer compensation from Bosch BMP3 Sensor API (BST-BMP390-DS002)
// Returns temperature in units of 0.01 degrees C
static int64_t prv_compensate_temperature(uint32_t uncomp_temp) {
  int64_t partial_data1 = (int64_t)(uncomp_temp - ((int64_t)256 * s_calib.par_t1));
  int64_t partial_data2 = (int64_t)(s_calib.par_t2 * partial_data1);
  int64_t partial_data3 = (int64_t)(partial_data1 * partial_data1);
  int64_t partial_data4 = (int64_t)partial_data3 * s_calib.par_t3;
  int64_t partial_data5 = (int64_t)((int64_t)(partial_data2 * 262144) + partial_data4);
  int64_t partial_data6 = (int64_t)(partial_data5 / 4294967296);

  // Store t_lin for pressure compensation
  s_calib.t_lin = partial_data6;

  return (int64_t)((partial_data6 * 25) / 16384);
}

// Integer compensation from Bosch BMP3 Sensor API (BST-BMP390-DS002)
// Returns pressure in units of Pa * 100 (i.e. 1/100 Pa)
static uint64_t prv_compensate_pressure(uint32_t uncomp_press) {
  int64_t partial_data1 = s_calib.t_lin * s_calib.t_lin;
  int64_t partial_data2 = partial_data1 / 64;
  int64_t partial_data3 = (partial_data2 * s_calib.t_lin) / 256;
  int64_t partial_data4 = (s_calib.par_p8 * partial_data3) / 32;
  int64_t partial_data5 = (s_calib.par_p7 * partial_data1) * 16;
  int64_t partial_data6 = (s_calib.par_p6 * s_calib.t_lin) * 4194304;
  int64_t offset = (s_calib.par_p5 * (int64_t)140737488355328) +
                   partial_data4 + partial_data5 + partial_data6;

  partial_data2 = (s_calib.par_p4 * partial_data3) / 32;
  partial_data4 = (s_calib.par_p3 * partial_data1) * 4;
  partial_data5 = ((int64_t)s_calib.par_p2 - (int64_t)16384) * s_calib.t_lin * 2097152;
  int64_t sensitivity = (((int64_t)s_calib.par_p1 - (int64_t)16384) * (int64_t)70368744177664) +
                        partial_data2 + partial_data4 + partial_data5;

  partial_data1 = (sensitivity / 16777216) * uncomp_press;
  partial_data2 = s_calib.par_p10 * s_calib.t_lin;
  partial_data3 = partial_data2 + ((int64_t)65536 * s_calib.par_p9);
  partial_data4 = (partial_data3 * uncomp_press) / 8192;
  // Split to avoid overflow
  partial_data5 = (uncomp_press * (partial_data4 / 10)) / 512;
  partial_data5 = partial_data5 * 10;
  partial_data6 = (int64_t)uncomp_press * (int64_t)uncomp_press;
  partial_data2 = (s_calib.par_p11 * partial_data6) / 65536;
  partial_data3 = (partial_data2 * uncomp_press) / 128;
  partial_data4 = (offset / 4) + partial_data1 + partial_data5 + partial_data3;

  return (uint64_t)((uint64_t)partial_data4 * 25) / (uint64_t)1099511627776;
}

void pressure_init(void) {
  uint8_t chip_id;
  s_initialized = false;
  s_filter_mode = PRESSURE_FILTER_NONE;

  if (!s_mutex) {
    s_mutex = mutex_create();
  }

  if (!prv_read_register(BMP390_REG_CHIP_ID, &chip_id) ||
      chip_id != BMP390_CHIP_ID_VALUE) {
    PBL_LOG_DBG("BMP390 probe failed; chip_id 0x%02x", chip_id);
    return;
  }

  PBL_LOG_DBG("BMP390 found, chip_id=0x%02x", chip_id);

  // Soft reset to known state
  if (!prv_write_register(BMP390_REG_CMD, 0xB6)) {
    PBL_LOG_DBG("BMP390 soft reset FAILED");
  }

  // Poll status register for cmd_rdy (bit 4) after reset, with timeout
  for (int i = 0; i < 50; i++) {
    delay_us(1000);
    uint8_t status;
    if (prv_read_register(0x03, &status) && (status & 0x10)) {
      PBL_LOG_DBG("BMP390 ready after %d ms", i + 1);
      break;
    }
  }

  // Read factory calibration data
  uint8_t calib_raw[BMP390_CALIB_DATA_LEN];
  if (!prv_read_register_block(BMP390_REG_CALIB_DATA, BMP390_CALIB_DATA_LEN, calib_raw)) {
    PBL_LOG_DBG("BMP390 calibration read failed");
    return;
  }
  prv_parse_calib_data(calib_raw);

  // Configure: 8x pressure oversampling, 1x temperature, IIR off by default
  prv_write_register(BMP390_REG_OSR, BMP390_OSR_P_8X | BMP390_OSR_T_1X);
  prv_write_register(BMP390_REG_CONFIG, BMP390_IIR_COEFF_0);

  // Enable pressure + temperature sensors (stay in sleep — reads trigger forced mode)
  prv_write_register(BMP390_REG_PWR_CTRL, BMP390_PRESS_EN | BMP390_TEMP_EN);

  s_initialized = true;
  PBL_LOG_DBG("BMP390 initialized");
}

// Trigger a forced-mode measurement and wait for data ready
static bool prv_trigger_measurement(void) {
  // Trigger forced measurement
  prv_write_register(BMP390_REG_PWR_CTRL,
                     BMP390_PRESS_EN | BMP390_TEMP_EN | BMP390_MODE_FORCED);

  // Wait for data ready (8x oversampling @ ~5ms conversion = ~40ms typical)
  for (int i = 0; i < 100; i++) {
    delay_us(1000);
    uint8_t int_status;
    if (prv_read_register(BMP390_REG_INT_STATUS, &int_status) &&
        (int_status & BMP390_DRDY)) {
      return true;
    }
  }
  return false;
}

bool pressure_read(int32_t *pressure_pa, int32_t *temperature_centideg) {
  if (!s_initialized) {
    return false;
  }

  mutex_lock(s_mutex);

  // Trigger forced-mode measurement and wait for completion
  if (!prv_trigger_measurement()) {
    mutex_unlock(s_mutex);
    return false;
  }

  // Read 6 bytes of sensor data: pressure[0:2], temperature[0:2]
  uint8_t data[BMP390_DATA_LEN];
  if (!prv_read_register_block(BMP390_REG_DATA, BMP390_DATA_LEN, data)) {
    mutex_unlock(s_mutex);
    return false;
  }

  uint32_t uncomp_press = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16);
  uint32_t uncomp_temp = (uint32_t)data[3] | ((uint32_t)data[4] << 8) | ((uint32_t)data[5] << 16);

  // Temperature must be compensated first — it sets t_lin used by pressure compensation
  int64_t temp = prv_compensate_temperature(uncomp_temp);
  uint64_t press = prv_compensate_pressure(uncomp_press);

  mutex_unlock(s_mutex);

  // temp is in 0.01 degC, press is in Pa * 100
  if (temperature_centideg) {
    *temperature_centideg = (int32_t)temp;
  }
  if (pressure_pa) {
    *pressure_pa = (int32_t)(press / 100);  // Convert to Pa
  }

  return true;
}

int32_t pressure_get_altitude_cm(int32_t pressure_pa, int32_t sea_level_pa) {
  // Barometric formula approximation using integer math:
  // altitude = 44330 * (1 - (P/P0)^0.1903)
  //
  // For integer approximation, use a first-order Taylor expansion around P0:
  // altitude ≈ (P0 - P) * 8435 / P0   (in cm, valid within ~1000m of sea level)
  //
  // 8435 comes from: 44330 * 0.1903 * 100 (cm conversion) ≈ 843500 / 100
  // More precisely: dh/dP at sea level = -RT/(Mg) ≈ -8.43m/hPa = -0.0843m/Pa
  // In cm: 8.43 cm per Pa of pressure difference

  if (sea_level_pa <= 0) {
    return 0;
  }

  int64_t delta = (int64_t)(sea_level_pa - pressure_pa);
  return (int32_t)((delta * 843500) / sea_level_pa);
}

// Lookup table for full barometric formula: h = 44330 * (1 - (P/P0)^0.190263)
// Index i: altitude_cm for pressure ratio (1024 - i*5) / 1024
// 129 entries covering 0–7547m
static const int32_t s_altitude_table[] = {
        0,    4127,    8269,   12429,   16605,   20798,   25008,   29236,
    33480,   37743,   42022,   46320,   50636,   54969,   59322,   63692,
    68082,   72490,   76917,   81364,   85829,   90315,   94820,   99346,
   103891,  108457,  113044,  117651,  122279,  126929,  131600,  136293,
   141008,  145744,  150504,  155286,  160090,  164918,  169769,  174644,
   179543,  184465,  189412,  194384,  199381,  204403,  209451,  214524,
   219623,  224749,  229902,  235081,  240288,  245523,  250785,  256076,
   261396,  266744,  272122,  277530,  282968,  288436,  293935,  299466,
   305028,  310622,  316249,  321909,  327602,  333328,  339089,  344885,
   350716,  356583,  362486,  368425,  374402,  380416,  386468,  392560,
   398690,  404861,  411072,  417324,  423618,  429954,  436333,  442756,
   449223,  455735,  462293,  468897,  475548,  482247,  488995,  495793,
   502640,  509539,  516490,  523494,  530552,  537664,  544832,  552057,
   559339,  566681,  574082,  581543,  589067,  596655,  604306,  612023,
   619807,  627660,  635582,  643574,  651640,  659779,  667993,  676285,
   684655,  693106,  701638,  710255,  718957,  727746,  736626,  745597,
   754662,
};

#define ALTITUDE_TABLE_ENTRIES 129
#define ALTITUDE_TABLE_STEP   5  // ratio step per index (at 1024 scale)

// Use 18-bit precision for the pressure ratio instead of 10-bit.
// At 10 bits, 1 LSB ≈ 98 Pa ≈ 8.3m (27ft) — way too coarse.
// At 18 bits, 1 LSB ≈ 0.38 Pa ≈ 3cm — smooth enough for an altimeter.
#define RATIO_SHIFT      18
#define RATIO_SCALE      (1 << RATIO_SHIFT)           // 262144
#define PRECISION_MULT   (RATIO_SCALE / 1024)          // 256
#define TABLE_STEP_SCALED (ALTITUDE_TABLE_STEP * PRECISION_MULT)  // 1280

int32_t pressure_get_altitude_full_cm(int32_t pressure_pa, int32_t ref_pressure_pa) {
  if (ref_pressure_pa <= 0 || pressure_pa <= 0) {
    return 0;
  }

  // Compute pressure ratio in 1/262144 units: ratio = P * 2^18 / P0
  uint32_t ratio = (uint32_t)(((uint64_t)pressure_pa << RATIO_SHIFT) / ref_pressure_pa);

  // Determine sign: if pressure > reference, altitude is negative (below reference)
  bool negative = (pressure_pa > ref_pressure_pa);
  if (negative) {
    // Mirror: compute altitude for the inverse ratio
    ratio = (uint32_t)(((uint64_t)ref_pressure_pa << RATIO_SHIFT) / pressure_pa);
  }

  // Clamp to table range
  if (ratio >= RATIO_SCALE) {
    return 0;
  }
  uint32_t inv = RATIO_SCALE - ratio;  // 0 at reference, increases with altitude

  // Table lookup with linear interpolation
  uint32_t idx = inv / TABLE_STEP_SCALED;
  uint32_t frac = inv % TABLE_STEP_SCALED;

  if (idx >= ALTITUDE_TABLE_ENTRIES - 1) {
    return negative ? -s_altitude_table[ALTITUDE_TABLE_ENTRIES - 1]
                    : s_altitude_table[ALTITUDE_TABLE_ENTRIES - 1];
  }

  int32_t alt_low = s_altitude_table[idx];
  int32_t alt_high = s_altitude_table[idx + 1];
  int32_t altitude = alt_low + ((int64_t)(alt_high - alt_low) * (int32_t)frac) / TABLE_STEP_SCALED;

  return negative ? -altitude : altitude;
}

// IIR coefficients for each ODR when filtering is enabled
static uint8_t prv_iir_for_odr(PressureODR odr) {
  switch (odr) {
    case PRESSURE_ODR_1HZ:  return BMP390_IIR_COEFF_7;
    case PRESSURE_ODR_5HZ:  return BMP390_IIR_COEFF_7;
    case PRESSURE_ODR_10HZ: return BMP390_IIR_COEFF_7;
    case PRESSURE_ODR_25HZ: return BMP390_IIR_COEFF_15;
    case PRESSURE_ODR_50HZ: return BMP390_IIR_COEFF_31;
    default:                return BMP390_IIR_COEFF_0;
  }
}

bool pressure_set_odr(PressureODR odr) {
  if (!s_initialized) {
    return false;
  }

  mutex_lock(s_mutex);

  // Each ODR preset configures: ODR register, oversampling, and IIR filter.
  // Higher rates need lower oversampling to keep up with the output data rate.
  uint8_t odr_reg, osr_reg;
  switch (odr) {
    case PRESSURE_ODR_1HZ:
      odr_reg = BMP390_ODR_1P5625HZ;
      osr_reg = BMP390_OSR_P_8X | BMP390_OSR_T_1X;
      break;
    case PRESSURE_ODR_5HZ:
      odr_reg = BMP390_ODR_6P25HZ;
      osr_reg = BMP390_OSR_P_8X | BMP390_OSR_T_1X;
      break;
    case PRESSURE_ODR_10HZ:
      odr_reg = BMP390_ODR_12P5HZ;
      osr_reg = BMP390_OSR_P_4X | BMP390_OSR_T_1X;
      break;
    case PRESSURE_ODR_25HZ:
      odr_reg = BMP390_ODR_25HZ;
      osr_reg = BMP390_OSR_P_2X | BMP390_OSR_T_1X;
      break;
    case PRESSURE_ODR_50HZ:
      odr_reg = BMP390_ODR_50HZ;
      osr_reg = BMP390_OSR_P_1X | BMP390_OSR_T_1X;
      break;
    default:
      mutex_unlock(s_mutex);
      return false;
  }

  uint8_t config_reg = (s_filter_mode == PRESSURE_FILTER_SMOOTH)
                        ? prv_iir_for_odr(odr) : BMP390_IIR_COEFF_0;

  // Switch to sleep to apply config cleanly, then back to normal
  prv_write_register(BMP390_REG_PWR_CTRL, BMP390_PRESS_EN | BMP390_TEMP_EN);
  prv_write_register(BMP390_REG_ODR, odr_reg);
  prv_write_register(BMP390_REG_OSR, osr_reg);
  prv_write_register(BMP390_REG_CONFIG, config_reg);
  prv_write_register(BMP390_REG_PWR_CTRL,
                     BMP390_PRESS_EN | BMP390_TEMP_EN | BMP390_MODE_NORMAL);

  mutex_unlock(s_mutex);
  return true;
}

bool pressure_set_filter_mode(PressureFilterMode mode) {
  if (!s_initialized) {
    return false;
  }

  mutex_lock(s_mutex);
  s_filter_mode = mode;

  // Apply immediately: update the CONFIG register
  uint8_t config_reg = (mode == PRESSURE_FILTER_SMOOTH)
                        ? BMP390_IIR_COEFF_7 : BMP390_IIR_COEFF_0;
  prv_write_register(BMP390_REG_CONFIG, config_reg);

  mutex_unlock(s_mutex);
  return true;
}

void command_pressure_read(void) {
  int32_t pressure_pa = 0;
  int32_t temperature_centideg = 0;

  if (pressure_read(&pressure_pa, &temperature_centideg)) {
    char buffer[64];
    prompt_send_response_fmt(buffer, sizeof(buffer),
                             "Pressure: %" PRId32 " Pa", pressure_pa);
    prompt_send_response_fmt(buffer, sizeof(buffer),
                             "Temp: %" PRId32 ".%02" PRId32 " C",
                             temperature_centideg / 100,
                             temperature_centideg >= 0
                               ? temperature_centideg % 100
                               : (-temperature_centideg) % 100);
    int32_t alt_cm = pressure_get_altitude_cm(pressure_pa, 101325);
    prompt_send_response_fmt(buffer, sizeof(buffer),
                             "Alt (est): %" PRId32 ".%02" PRId32 " m",
                             alt_cm / 100,
                             (alt_cm >= 0 ? alt_cm : -alt_cm) % 100);
  } else {
    prompt_send_response("Pressure read FAILED");
  }
}

void command_pressure_reinit(void) {
  char buffer[80];

  // Read chip ID
  uint8_t chip_id;
  bool ok = prv_read_register(BMP390_REG_CHIP_ID, &chip_id);
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "Chip ID: 0x%02x (read %s)", chip_id, ok ? "ok" : "FAIL");

  // Write PWR_CTRL: enable sensors + normal mode
  uint8_t pwr = BMP390_PRESS_EN | BMP390_TEMP_EN | BMP390_MODE_NORMAL;
  prv_write_register(BMP390_REG_PWR_CTRL, pwr);
  uint8_t readback;
  prv_read_register(BMP390_REG_PWR_CTRL, &readback);
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "PWR_CTRL: wrote 0x%02x read 0x%02x", pwr, readback);

  // If mode bits didn't stick, try forced mode
  if ((readback & 0x30) == 0) {
    uint8_t forced = BMP390_PRESS_EN | BMP390_TEMP_EN | BMP390_MODE_FORCED;
    prv_write_register(BMP390_REG_PWR_CTRL, forced);
    prv_read_register(BMP390_REG_PWR_CTRL, &readback);
    prompt_send_response_fmt(buffer, sizeof(buffer),
                             "Forced: wrote 0x%02x read 0x%02x", forced, readback);

    // Wait for measurement in forced mode
    delay_us(100000);

    prv_read_register(BMP390_REG_PWR_CTRL, &readback);
    uint8_t int_status;
    prv_read_register(BMP390_REG_INT_STATUS, &int_status);
    prompt_send_response_fmt(buffer, sizeof(buffer),
                             "After wait: PWR_CTRL=0x%02x DRDY=%d",
                             readback, (int_status & BMP390_DRDY) ? 1 : 0);
  }

  // Read data
  uint8_t data[BMP390_DATA_LEN];
  prv_read_register_block(BMP390_REG_DATA, BMP390_DATA_LEN, data);
  uint32_t p = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16);
  uint32_t t = (uint32_t)data[3] | ((uint32_t)data[4] << 8) | ((uint32_t)data[5] << 16);
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "Raw P: %" PRIu32 " T: %" PRIu32, p, t);

  // OSR readback
  uint8_t osr_rb;
  prv_read_register(BMP390_REG_OSR, &osr_rb);
  prompt_send_response_fmt(buffer, sizeof(buffer), "OSR: 0x%02x", osr_rb);
}

void command_pressure_debug(void) {
  if (!s_initialized) {
    prompt_send_response("BMP390 not initialized");
    return;
  }

  char buffer[80];

  // Read raw data
  uint8_t data[BMP390_DATA_LEN];
  if (!prv_read_register_block(BMP390_REG_DATA, BMP390_DATA_LEN, data)) {
    prompt_send_response("Raw read failed");
    return;
  }

  uint32_t uncomp_press = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16);
  uint32_t uncomp_temp = (uint32_t)data[3] | ((uint32_t)data[4] << 8) | ((uint32_t)data[5] << 16);

  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "Raw P: %" PRIu32 " T: %" PRIu32, uncomp_press, uncomp_temp);

  // Read PWR_CTRL to verify sensor mode
  uint8_t pwr_ctrl;
  prv_read_register(BMP390_REG_PWR_CTRL, &pwr_ctrl);
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "PWR_CTRL: 0x%02x", pwr_ctrl);

  // Read INT_STATUS to check data ready
  uint8_t int_status;
  prv_read_register(BMP390_REG_INT_STATUS, &int_status);
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "INT_STATUS: 0x%02x (DRDY=%d)", int_status,
                           (int_status & BMP390_DRDY) ? 1 : 0);

  // Dump key calibration coefficients
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "par_t1=%u par_t2=%u par_t3=%d",
                           s_calib.par_t1, s_calib.par_t2, s_calib.par_t3);
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "par_p1=%d par_p2=%d par_p3=%d par_p4=%d",
                           s_calib.par_p1, s_calib.par_p2, s_calib.par_p3, s_calib.par_p4);
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "par_p5=%u par_p6=%u par_p7=%d par_p8=%d",
                           s_calib.par_p5, s_calib.par_p6, s_calib.par_p7, s_calib.par_p8);
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "par_p9=%d par_p10=%d par_p11=%d",
                           s_calib.par_p9, s_calib.par_p10, s_calib.par_p11);
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "t_lin=%" PRId64, s_calib.t_lin);
}
