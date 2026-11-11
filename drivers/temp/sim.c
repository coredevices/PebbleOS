/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/temp.h>
#include <pbl/drivers/temp/sim.h>

static int32_t s_millicelsius = 25000;

void pbl_temp_sim_set(const struct pbl_temp *temp, int32_t millicelsius) {
  s_millicelsius = millicelsius;
}

static int prv_read(const struct pbl_temp *temp, int32_t *millicelsius) {
  *millicelsius = s_millicelsius;

  return 0;
}

static const struct pbl_temp_ops s_ops = {
  .read = prv_read,
};

PBL_TEMP_STATE_DEFINE(pbl_temp_sim);
const struct pbl_temp pbl_temp_sim = PBL_TEMP_INIT(pbl_temp_sim, "temp", nullptr, nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_temp_sim, &pbl_temp_sim.dev);
