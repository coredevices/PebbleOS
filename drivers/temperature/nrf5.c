/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/drivers/temperature.h>
#include <pbl/drivers/temperature/nrf5.h>
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

static int prv_read(const struct pbl_temp *temp, int32_t *millicelsius) {
  int ret;

  prv_clear_datardy();
  prv_putreg(NRF52_TEMP_TASKS_START_OFFSET, 1U);

  ret = PBL_POLL_BUSY_US(prv_getreg(NRF52_TEMP_EVENTS_DATARDY_OFFSET) != 0U, 1,
                         CONVERSION_TIMEOUT_US);
  if (ret == 0) {
    *millicelsius = (int32_t)prv_getreg(NRF52_TEMP_TEMP_OFFSET) * MILLIDEGREES_PER_LSB;
  }

  prv_clear_datardy();
  prv_putreg(NRF52_TEMP_TASKS_STOP_OFFSET, 1U);

  return ret;
}

static const struct pbl_temp_ops s_ops = {
  .read = prv_read,
};

PBL_TEMP_STATE_DEFINE(pbl_temp_nrf5);
const struct pbl_temp pbl_temp_nrf5 =
    PBL_TEMP_INIT(pbl_temp_nrf5, "temp", nullptr, nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_temp_nrf5, &pbl_temp_nrf5.dev);
