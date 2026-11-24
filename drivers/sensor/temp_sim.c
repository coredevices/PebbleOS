/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/sensor.h>
#include <pbl/drivers/sensor/sim.h>
#include <pbl/kernel/time.h>
#include <pbl/util/byteorder.h>
#include <pbl/util/size.h>

static const struct pbl_sensor_chan s_chans[] = {
  PBL_SENSOR_CHAN(PBL_SENSOR_TEMP, 0, 0, PBL_SENSOR_FMT_S32_LE, 1, 1, 0),
};

static int32_t s_millicelsius = 25000;

void pbl_temp_sim_set(const struct pbl_sensor *sensor, int32_t millicelsius) {
  s_millicelsius = millicelsius;
}

static int prv_read(const struct pbl_sensor *sensor, enum pbl_sensor_type type,
                    struct pbl_sensor_frame *frame) {
  pbl_put_le32((uint32_t)s_millicelsius, frame->data);
  frame->chans = s_chans;
  frame->num_chans = PBL_ARRAY_SIZE(s_chans);
  frame->timestamp_us = pbl_uptime_us();

  return 0;
}

static const struct pbl_sensor_ops s_ops = {
  .read = prv_read,
};

PBL_SENSOR_STATE_DEFINE(pbl_temp_sim);
const struct pbl_sensor pbl_temp_sim = PBL_SENSOR_INIT(
    pbl_temp_sim, "temp", nullptr, &s_ops, PBL_BIT(PBL_SENSOR_TEMP), 0U, PBL_SENSOR_MOUNT_IDENTITY);
PBL_DEVICE_REGISTER(pbl_temp_sim, &pbl_temp_sim.dev);
