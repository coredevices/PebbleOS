/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/charger.h>
#include <pbl/kernel/delay.h>

#define READS 3

static const char *const s_status[] = {
  [PBL_CHARGER_STATUS_UNKNOWN] = "unknown",
  [PBL_CHARGER_STATUS_FULL] = "full",
  [PBL_CHARGER_STATUS_TRICKLE] = "trickle",
  [PBL_CHARGER_STATUS_CONSTANT_CURRENT] = "constant current",
  [PBL_CHARGER_STATUS_CONSTANT_VOLTAGE] = "constant voltage",
};

static void prv_online_changed(const struct pbl_charger *charger) {
  bool online = false;

  (void)pbl_charger_is_online(charger, &online);
  printf("charger: external power %s\n", online ? "connected" : "removed");
}

static const struct pbl_charger_listener s_listener = {
  .online_changed = prv_online_changed,
};

int main(void) {
  /* Called from a work queue whenever external power comes or goes. */
  pbl_charger_set_listener(CHARGER, &s_listener);

  for (int i = 0; i < READS; i++) {
    struct pbl_charger_measurements meas;
    enum pbl_charger_status status;
    bool online;
    int ret;

    ret = pbl_charger_get_measurements(CHARGER, &meas);
    if (ret != 0) {
      fprintf(stderr, "charger: measurement failed (%d)\n", ret);
      return ret;
    }

    ret = pbl_charger_is_online(CHARGER, &online);
    if (ret != 0) {
      fprintf(stderr, "charger: external power read failed (%d)\n", ret);
      return ret;
    }

    ret = pbl_charger_get_status(CHARGER, &status);
    if (ret != 0) {
      fprintf(stderr, "charger: charge status failed (%d)\n", ret);
      return ret;
    }

    printf("charger: %d mV, %d uA, %d mC\n", (int)meas.voltage_mv, (int)meas.current_ua,
           (int)meas.temp_mc);
    printf("charger: external power %s, charge %s\n", online ? "present" : "absent",
           s_status[status]);

    pbl_sleep_ms(1000);
  }

  pbl_charger_set_listener(CHARGER, nullptr);

  printf("Sample done\n");

  return 0;
}
