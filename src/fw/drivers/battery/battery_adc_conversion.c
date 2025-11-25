/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "drivers/battery.h"

uint32_t battery_convert_reading_to_millivolts(ADCVoltageMonitorReading reading,
                                               uint32_t numerator, uint32_t denominator) {
  // The result from the ADC is 0-1.8V, but scaled into a 12bit number. That means a value of
  // zero indicates 0V and a value of 4095 (2^12 - 1) indicates a reading of 1.8V.

  // The ADC is only capable of measuring between 0 and 1.8V, so we expect the thing providing
  // a voltage to the monitor pin to be scaling it in some way. This scaling factor is captured
  // in the numerator and denominator arguments.

  // Therefore, whatever 12-bit number we read from the ADC needs to be converted to a voltage by
  // multiplying it by 1.8/4095, and then further scaled into it's final voltage by multiplying by
  // numerator and dividing by the denominator.

  // Finally, our reading contains a sum of many readings from both the monitor pin as well as
  // an internal 1.2V reference voltage. The reason for this is that these pins will have noise
  // on them and we can assume that any ripple on the mon rail will also occur on the 1.2V internal
  // reference voltage. So, we can measure the voltage synchronously on both ADCs and then
  // calculate a relative voltage. Therefore, the actual monitor voltage can be estimated by
  // calculating 1800 * (vmon_mv_sum / (vref_mv_sum * 1800 / 1200) or
  // 1800 * (vmon_mv_sum * 1200) / (vref_mv_sum * 1800).

  // Convert from 12-bit to millivolts by multiplying by 1800/4095 which is the same as 40/91
  const uint32_t vref_mv_sum = reading.vref_total * 40 / 91;
  const uint32_t vmon_mv_sum = reading.vmon_total * 40 / 91;

  // Use the reference voltage to convert a single smoothed out mv reading.
  // Multiply vmon/vref * 2/3 to find a percentage of the full scale and then multiply it back
  // by 1800 to get back to mV.
  const uint32_t millivolts = ((vmon_mv_sum * 1800 * 2) / (vref_mv_sum * 3));

  // Finally, hit it with the scaling factors.
  const uint32_t scaled_millivolts = millivolts * numerator / denominator;

  return scaled_millivolts;
}
