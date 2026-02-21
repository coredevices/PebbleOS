/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "drivers/accel.h"
#include "drivers/rtc.h"
#include "services/common/regular_timer.h"

#define LIS2DW12_FIFO_SIZE 32
#define LIS2DW12_SAMPLE_SIZE_BYTES 6

typedef struct LIS2DW12State {
  bool initialized;
  bool rotated;
  bool shake_detection_enabled;
  bool double_tap_detection_enabled;
  uint32_t sampling_interval_us;
  uint8_t num_samples;
  uint8_t raw_sample_buf[LIS2DW12_FIFO_SIZE * LIS2DW12_SAMPLE_SIZE_BYTES];
  RegularTimerInfo int1_wdt_timer;
  RtcTicks last_int1_tick;
  uint32_t int1_period_ms;
  uint32_t num_recoveries;
  uint8_t wk_ths_curr;
} LIS2DW12State;

typedef struct LIS2DW12Config {
  //! Driver state
  LIS2DW12State *state;
  //! I2C slave port configuration
  I2CSlavePort i2c;
  //! INT1 EXTI configuration
  ExtiConfig int1;
  //! Disable ADDR pull-up resistor
  bool disable_addr_pullup;
  //! Default wake duration (0-3)
  uint8_t wk_dur_default;
  //! Default wake threshold (wk_ths_min, wk_ths_max)
  uint8_t wk_ths_default;
  //! Minimum wake threshold (1-63)
  uint8_t wk_ths_min;
  //! Maximum wake threshold (1-63)
  uint8_t wk_ths_max;
  //! Scale (+/-2000, 4000, 8000 or 16000 mg)
  uint16_t scale_mg;
  //! FIFO threshold (1-32)
  //! FIFO threshold should be chosen so that FIFO can be drained before overrun
  //! occurs at max ODR (200 Hz in this implementation). At maximum rate, a new
  //! sample is available every 5 ms, and the FIFO can hold up to 32 samples.
  //! FIFO drain takes 29 + 9 * 32 * 6 clocks. For example, at 400 kHz I2C clock,
  //! this is ~4.4ms to drain a full FIFO. Choosing a threshold of 16 samples,
  //! would give 16 / 200 Hz = 80 ms to drain the FIFO before overrun occurs.
  uint8_t fifo_threshold;
  //! Axis mapping (0: X, 1: Y, 2: Z)
  uint8_t axis_map[3];
  //! Axis direction (1 upside, -1 downside)
  int8_t axis_dir[3];
} LIS2DW12Config;