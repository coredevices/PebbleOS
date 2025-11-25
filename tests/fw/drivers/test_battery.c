/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "drivers/battery.h"

static uint32_t prv_convert_millivolts_to_12bit_reading(int millivolts) {
  return 4095 * millivolts / 1800;
}

#define VREF_VOLTAGE 1200

void test_battery__reading_conversion_boring(void) {
  ADCVoltageMonitorReading reading = {
    .vref_total = prv_convert_millivolts_to_12bit_reading(VREF_VOLTAGE),
    .vmon_total = prv_convert_millivolts_to_12bit_reading(1800)
  };

  uint32_t result = battery_convert_reading_to_millivolts(reading, 1, 1);
  cl_assert_equal_i(result, 1800);

  reading.vmon_total = prv_convert_millivolts_to_12bit_reading(1200);

  result = battery_convert_reading_to_millivolts(reading, 1, 1);
  cl_assert_equal_i(result, 1200);

  reading.vmon_total = prv_convert_millivolts_to_12bit_reading(0);

  result = battery_convert_reading_to_millivolts(reading, 1, 1);
  cl_assert_equal_i(result, 0);
}

void test_battery__reading_conversion_40_samples(void) {
  ADCVoltageMonitorReading reading = {
    .vref_total = prv_convert_millivolts_to_12bit_reading(VREF_VOLTAGE) * 40,
    .vmon_total = prv_convert_millivolts_to_12bit_reading(1800) * 40
  };

  uint32_t result = battery_convert_reading_to_millivolts(reading, 1, 1);
  cl_assert_equal_i(result, 1800);

  reading.vmon_total = prv_convert_millivolts_to_12bit_reading(1200) * 40;

  result = battery_convert_reading_to_millivolts(reading, 1, 1);
  cl_assert_equal_i(result, 1200);

  reading.vmon_total = prv_convert_millivolts_to_12bit_reading(0);

  result = battery_convert_reading_to_millivolts(reading, 1, 1);
  cl_assert_equal_i(result, 0);
}

// Make sure our new method for calculating battery usage matches the old one for stm32f2
///////////////////////////////////////////////////////////
static int prv_legacy_f2_calculation_millivolts(ADCVoltageMonitorReading reading) {
  return (int) ((reading.vmon_total * (2730) / reading.vref_total) * 295) / 256;
}

void test_battery__reading_conversion_f2(void) {
  ADCVoltageMonitorReading reading = {
    .vref_total = prv_convert_millivolts_to_12bit_reading(VREF_VOLTAGE),
    .vmon_total = prv_convert_millivolts_to_12bit_reading(1800)
  };

  uint32_t result = battery_convert_reading_to_millivolts(reading, 3599, 1373);
  cl_assert_equal_i(result, prv_legacy_f2_calculation_millivolts(reading));
}


// Make sure our new method for calculating battery usage matches the old one for stm32f4
///////////////////////////////////////////////////////////
static int prv_legacy_f4_calculation_millivolts(ADCVoltageMonitorReading reading) {
  return (int) (((reading.vmon_total * (2730) / reading.vref_total) * 120) / 91);
}

void test_battery__reading_conversion_f4(void) {
  ADCVoltageMonitorReading reading = {
    .vref_total = prv_convert_millivolts_to_12bit_reading(VREF_VOLTAGE),
    .vmon_total = prv_convert_millivolts_to_12bit_reading(1800)
  };

  uint32_t result = battery_convert_reading_to_millivolts(reading, 3, 1);
  cl_assert_equal_i(result, prv_legacy_f4_calculation_millivolts(reading));
}

