/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/sensor.h>
#include <pbl/drivers/sensor/temp_nrf5.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/mmio.h>
#include <pbl/kernel/time.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <hardware/nrf52_temp.h>

#define CONVERSION_TIMEOUT_US 100U

#define MILLIDEGREES_PER_LSB 250

static const struct pbl_sensor_chan s_chans[] = {
  PBL_SENSOR_CHAN(PBL_SENSOR_TEMP, 0, 0, PBL_SENSOR_FMT_S32_LE, MILLIDEGREES_PER_LSB, 1, 0),
};

static inline uint32_t prv_getreg(const struct pbl_temp_nrf5 *nrf5, uint32_t offset) {
  return pbl_readl(nrf5->base + offset);
}

static inline void prv_putreg(const struct pbl_temp_nrf5 *nrf5, uint32_t offset, uint32_t value) {
  pbl_writel(value, nrf5->base + offset);
}

static void prv_clear_datardy(const struct pbl_temp_nrf5 *nrf5) {
  prv_putreg(nrf5, NRF52_TEMP_EVENTS_DATARDY_OFFSET, 0U);
  (void)prv_getreg(nrf5, NRF52_TEMP_EVENTS_DATARDY_OFFSET);
}

static int prv_read(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                    struct pbl_sensor_frame *frame) {
  const struct pbl_temp_nrf5 *nrf5 = PBL_CONTAINER_OF(sensor, const struct pbl_temp_nrf5, sensor);
  int ret;

  prv_clear_datardy(nrf5);
  prv_putreg(nrf5, NRF52_TEMP_TASKS_START_OFFSET, 1U);

  ret = PBL_POLL_BUSY_US(prv_getreg(nrf5, NRF52_TEMP_EVENTS_DATARDY_OFFSET) != 0U, 1,
                         CONVERSION_TIMEOUT_US);
  if (ret == 0) {
    pbl_put_le32(prv_getreg(nrf5, NRF52_TEMP_TEMP_OFFSET), frame->data);
    frame->chans = s_chans;
    frame->num_chans = PBL_ARRAY_SIZE(s_chans);
    frame->timestamp_us = pbl_uptime_us();
  }

  prv_clear_datardy(nrf5);
  prv_putreg(nrf5, NRF52_TEMP_TASKS_STOP_OFFSET, 1U);

  return ret;
}

const struct pbl_sensor_ops pbl_temp_nrf5_ops = {
  .read = prv_read,
};

#include PBL_DT_INSTANCES(nordic_nrf52832_temp)
