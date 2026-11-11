/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/als.h>
#include <pbl/drivers/als/sim.h>

static uint32_t s_level;

void pbl_als_sim_set(const struct pbl_als *als, uint32_t level) {
  s_level = level;
}

static int prv_init(const struct pbl_als *als) {
  return 0;
}

static int prv_read(const struct pbl_als *als, uint32_t *level) {
  *level = s_level;

  return 0;
}

static const struct pbl_als_ops s_ops = {
  .init = prv_init,
  .read = prv_read,
};

PBL_ALS_STATE_DEFINE(pbl_als_sim);
const struct pbl_als pbl_als_sim = PBL_ALS_INIT(pbl_als_sim, "als", nullptr, &s_ops, nullptr);
PBL_DEVICE_REGISTER(pbl_als_sim, &pbl_als_sim.dev);
