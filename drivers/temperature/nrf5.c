/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/temperature.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>

#include <hardware/nrf52_temp.h>

#define CONVERSION_TIMEOUT_US 100U

#define MILLIDEGREES_PER_LSB 250

static inline uint32_t prv_getreg(uint32_t offset) {
  return pbl_readl(NRF52_TEMP_BASE + offset);
}

static inline void prv_putreg(uint32_t offset, uint32_t value) {
  pbl_writel(value, NRF52_TEMP_BASE + offset);
}

static void prv_clear_datardy(void) {
  prv_putreg(NRF52_TEMP_EVENTS_DATARDY_OFFSET, 0U);
  (void)prv_getreg(NRF52_TEMP_EVENTS_DATARDY_OFFSET);
}

void temperature_init(void) {
}

int32_t temperature_read(void) {
  int32_t temp = INT32_MIN;

  prv_clear_datardy();
  prv_putreg(NRF52_TEMP_TASKS_START_OFFSET, 1U);

  for (uint32_t us = 0U; us < CONVERSION_TIMEOUT_US; us++) {
    if (prv_getreg(NRF52_TEMP_EVENTS_DATARDY_OFFSET) != 0U) {
      temp = (int32_t)prv_getreg(NRF52_TEMP_TEMP_OFFSET) * MILLIDEGREES_PER_LSB;
      break;
    }
    pbl_busy_wait_us(1);
  }

  prv_clear_datardy();
  prv_putreg(NRF52_TEMP_TASKS_STOP_OFFSET, 1U);

  return temp;
}
