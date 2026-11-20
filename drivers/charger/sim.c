/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/charger.h>
#include <pbl/drivers/charger/sim.h>
#include <pbl/util/math.h>

static uint8_t s_percent = 100;
static bool s_online;
// A full battery
static int32_t s_mv = 4230;

void pbl_charger_sim_set(uint8_t percent, int32_t voltage_mv, bool online) {
  bool changed = (online != s_online);

  s_percent = PBL_MIN(percent, 100);
  s_online = online;
  s_mv = voltage_mv;

  if (changed) {
    pbl_charger_report_online_changed(SIM_CHARGER);
  }
}

uint8_t pbl_charger_sim_get_percent(void) {
  return s_percent;
}

static int prv_is_online(const struct pbl_charger *charger, bool *online) {
  *online = s_online;

  return 0;
}

static int prv_get_status(const struct pbl_charger *charger, enum pbl_charger_status *status) {
  if (!s_online) {
    *status = PBL_CHARGER_STATUS_UNKNOWN;
  } else if (s_percent < 100) {
    *status = PBL_CHARGER_STATUS_CONSTANT_CURRENT;
  } else {
    *status = PBL_CHARGER_STATUS_FULL;
  }

  return 0;
}

static int prv_get_measurements(const struct pbl_charger *charger,
                                struct pbl_charger_measurements *meas) {
  meas->voltage_mv = s_mv;
  meas->current_ua = 100;
  meas->temp_mc = 25000;

  return 0;
}

static int prv_enable(const struct pbl_charger *charger, bool enable) {
  return 0;
}

static const struct pbl_charger_ops s_ops = {
  .is_online = prv_is_online,
  .get_status = prv_get_status,
  .get_measurements = prv_get_measurements,
  .enable = prv_enable,
};

PBL_CHARGER_STATE_DEFINE(pbl_charger_sim);
const struct pbl_charger pbl_charger_sim =
    PBL_CHARGER_INIT(pbl_charger_sim, "charger", nullptr, &s_ops);
PBL_DEVICE_REGISTER(pbl_charger_sim, &pbl_charger_sim.dev);
