/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/temperature.h>

#include <hal/nrf_temp.h>
#include <kernel/util/delay.h>
#include <nrfx.h>

#define CONVERSION_TIMEOUT_US 100U

#define MILLIDEGREES_PER_LSB 250

void temperature_init(void) {
}

int32_t temperature_read(void) {
  int32_t temp = INT32_MIN;

  nrf_temp_event_clear(NRF_TEMP, NRF_TEMP_EVENT_DATARDY);
  nrf_temp_task_trigger(NRF_TEMP, NRF_TEMP_TASK_START);

  for (uint32_t us = 0U; us < CONVERSION_TIMEOUT_US; us++) {
    if (nrf_temp_event_check(NRF_TEMP, NRF_TEMP_EVENT_DATARDY)) {
      temp = nrf_temp_result_get(NRF_TEMP) * MILLIDEGREES_PER_LSB;
      break;
    }
    delay_us(1);
  }

  nrf_temp_event_clear(NRF_TEMP, NRF_TEMP_EVENT_DATARDY);
  nrf_temp_task_trigger(NRF_TEMP, NRF_TEMP_TASK_STOP);

  return temp;
}
