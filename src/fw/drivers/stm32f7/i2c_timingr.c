/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "i2c_timingr.h"

#include "util/attributes.h"
#include "util/math.h"
#include "util/units.h"

static const struct {
  uint32_t clock_speed_max;
  uint32_t min_scl_low_ps;
  uint32_t min_scl_high_ps;
  uint32_t min_sda_setup_ps;
} s_timing_data[] = {
  [I2CBusMode_Standard] = {
    .clock_speed_max = 100000,
    .min_scl_low_ps = 4700000,
    .min_scl_high_ps = 4000000,
    .min_sda_setup_ps = 250000,
  },
  [I2CBusMode_FastMode] = {
    .clock_speed_max = 400000,
    .min_scl_low_ps = 1300000,
    .min_scl_high_ps = 600000,
    .min_sda_setup_ps = 100000,
  },
};

// Per the STM32F7 reference manual, the I2C peripheral adds 2-3 cycles to sync SCL with I2CCLK. In
// practice, 3 has been observed.
#define I2C_SYNC_CYCLES (3)

#define TIMINGR_MAX_SCLL (0x100) // 8 bits storing (SCLL - 1)
#define TIMINGR_MAX_SCLH (0x100) // 8 bits storing (SCLH - 1)
#define TIMINGR_MAX_SCLDEL (0x10) // 4 bits storing (SCLDEL - 1)
#define TIMINGR_MAX_PRESC (0x10) // 4 bits storing (PRESC - 1)

typedef union PACKED TIMINGR {
  struct {
    uint32_t SCLL:8;
    uint32_t SCLH:8;
    uint32_t SDADEL:4;
    uint32_t SCLDEL:4;
    uint32_t reserved:4;
    uint32_t PRESC:4;
  };
  uint32_t reg;
} TIMINGR;
_Static_assert(sizeof(TIMINGR) == sizeof(uint32_t), "Invalid TIMINGR size");

uint32_t i2c_timingr_calculate(uint32_t i2c_clk_frequency,
                               I2CBusMode bus_mode,
                               uint32_t target_bus_frequency,
                               uint32_t rise_time_ns,
                               uint32_t fall_time_ns) {
  if (bus_mode != I2CBusMode_Standard && bus_mode != I2CBusMode_FastMode) {
    // This is FM+ (or higher) and is not currently supported.
    return I2C_TIMINGR_INVALID_VALUE;
  }
  if (target_bus_frequency > s_timing_data[bus_mode].clock_speed_max) {
    return I2C_TIMINGR_INVALID_VALUE;
  }

  uint32_t min_scl_low_ps = s_timing_data[bus_mode].min_scl_low_ps;
  uint32_t min_scl_high_ps = s_timing_data[bus_mode].min_scl_high_ps;

  for (uint32_t prescaler = 1; prescaler <= TIMINGR_MAX_PRESC; ++prescaler) {
    const uint32_t base_frequency = i2c_clk_frequency / prescaler;
    const uint64_t base_period_ps = PS_PER_S / base_frequency;

    // Calculate what the total SCL period should be in terms of base clock cycles and then
    // recalculate the target frequency based on that value. The result will be the highest
    // target frequency we can obtain without going over.
    uint32_t total_scl_cycles = base_frequency / target_bus_frequency;

    // Calculate the number of overhead cycles. This includes the rise time, fall time, and sync
    // cycles for both SCLL and SCLH.
    const uint32_t overhead_i2cclk_cycles =
        DIVIDE_CEIL((rise_time_ns + fall_time_ns) * PS_PER_NS,
                    PS_PER_S / i2c_clk_frequency) +
        I2C_SYNC_CYCLES * 2;
    const uint32_t overhead_cycles = DIVIDE_CEIL(overhead_i2cclk_cycles, prescaler);

    // Figure out how many base clock cycles the minimum SCL periods correspond to.
    uint32_t scl_low = DIVIDE_CEIL(min_scl_low_ps, base_period_ps);
    uint32_t scl_high = DIVIDE_CEIL(min_scl_high_ps, base_period_ps);

    // Calculate the number of extra cycles we have.
    const int32_t extra_cycles = total_scl_cycles - scl_low - scl_high - overhead_cycles;
    if (extra_cycles < 0) {
      // The base frequency is too slow to satisfy the target frequency, and continuing will only
      // get slower, so give up.
      return I2C_TIMINGR_INVALID_VALUE;
    }

    // Split up the extra cycles evenly between the low and high periods. If necessary, give the
    // extra one to the high period arbitrarily.
    scl_low += extra_cycles / 2;
    scl_high += extra_cycles - (extra_cycles / 2);

    // Calculate the SDA setup time delay, which is confusingly referred to as SCLDEL.
    uint32_t scl_delay = DIVIDE_CEIL(
        (rise_time_ns * PS_PER_NS) + s_timing_data[bus_mode].min_sda_setup_ps,
        base_period_ps);

    // Check if the computed values are valid. If they aren't valid, we'll increase the prescaler
    // try again with a higher prescaler.
    // NOTE: We could end up in a situation where it is not valid, but could be if we split up the
    // extra cycles differently. We're not going to worry about this because it doesn't currently
    // happen in practice, and if it does, the next prescaler value will give us a valid (although
    // slightly sub-optimal) result.
    if ((scl_low <= TIMINGR_MAX_SCLL) && (scl_high <= TIMINGR_MAX_SCLH) &&
        (scl_delay <= TIMINGR_MAX_SCLDEL)) {
      // 1 less than the SCLL / SCLH / SCLDEL / PRESC values should be stored in the register.
      const TIMINGR result = (TIMINGR) {
        .SCLL = scl_low - 1,
        .SCLH = scl_high - 1,
        .SCLDEL = scl_delay - 1,
        .PRESC = prescaler - 1,
      };
      return result.reg;
    }
  }

  // We tried every possible prescaler and couldn't find valid TIMINGR values.
  return I2C_TIMINGR_INVALID_VALUE;
}
