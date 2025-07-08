/*
 * Copyright 2024 Google LLC
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
 */

#include "board/board.h"
#include "console/prompt.h"
#include "drivers/gpio.h"
#include "drivers/temperature.h"
#include "drivers/temperature/analog.h"
#include "drivers/voltage_monitor.h"
#include "drivers/periph_config.h"
#include "kernel/util/sleep.h"
#include "mfg/mfg_info.h"
#include "services/common/regular_timer.h"
#include "system/logging.h"
#include "system/passert.h"

#include <inttypes.h>

void temperature_init(void) {

}

int32_t temperature_read(void) {
  VoltageReading reading;
  voltage_monitor_read(TEMPERATURE_SENSOR->voltage_monitor, &reading);


  // See battery_adc_conversion.c for more details on how this works
  // convert from sum-of-12-bits to sum-of-mVs
  // by multiplying by 1800/4095 which is the same as 40/91
  const uint32_t vref_mv_sum = reading.vref_total * 40 / 91;
  const uint32_t vmon_mv_sum = reading.vmon_total * 40 / 91;

  // Multiply vmon/vref * 2/3 to find a percentage of the full scale and then multiply it back
  // by 1800 to get back to mV.
  int32_t millivolts = ((vmon_mv_sum * 1800 * 2) / (vref_mv_sum * 3));

  // convert to temperature (see stm32f4 ref manual, section 13.10)
  int32_t millidegreesC = ((millivolts - TEMPERATURE_SENSOR->millivolts_ref) *
                            TEMPERATURE_SENSOR->slope_denominator /
                            TEMPERATURE_SENSOR->slope_numerator) +
                           TEMPERATURE_SENSOR->millidegrees_ref;


  return millidegreesC;
}

void command_temperature_read(void) {
  char buffer[32];
  prompt_send_response_fmt(buffer, sizeof(buffer), "%"PRId32" ", temperature_read());
}
