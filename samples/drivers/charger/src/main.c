/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/charger.h>
#include <pbl/input/input.h>
#include <pbl/kernel/delay.h>

#define READS   30
#define READ_MS 1000
#define POLL_MS 100

static const char *const s_status[] = {
  [PBL_CHARGER_STATUS_UNKNOWN] = "unknown",
  [PBL_CHARGER_STATUS_FULL] = "full",
  [PBL_CHARGER_STATUS_TRICKLE] = "trickle",
  [PBL_CHARGER_STATUS_CONSTANT_CURRENT] = "constant current",
  [PBL_CHARGER_STATUS_CONSTANT_VOLTAGE] = "constant voltage",
};

static atomic_bool s_toggle;

static void prv_online_changed(const struct pbl_charger *charger) {
  bool online = false;

  (void)pbl_charger_is_online(charger, &online);
  printf("charger: external power %s\n", online ? "connected" : "removed");
}

static const struct pbl_charger_listener s_listener = {
  .online_changed = prv_online_changed,
};

/* Input callbacks may run in an interrupt: the main thread does the I2C work. */
static void prv_input_cb(const struct pbl_input_event *evt, void *user_data) {
  if (evt->type == PBL_INPUT_EV_KEY && evt->code == PBL_INPUT_KEY_SELECT && evt->value != 0) {
    atomic_store(&s_toggle, true);
  }
}

PBL_INPUT_CALLBACK_DEFINE(prv_input_cb, nullptr);

static int prv_set_charging(bool enable) {
  int ret = pbl_charger_enable(CHARGER, enable);

  if (ret != 0) {
    fprintf(stderr, "charger: could not %s charging (%d)\n", enable ? "enable" : "disable", ret);
    return ret;
  }

  printf("charger: charging %s\n", enable ? "enabled" : "disabled");

  return 0;
}

static int prv_read(void) {
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

  return 0;
}

int main(void) {
  bool charging = true;
  int ret = 0;

  /* Called from a work queue whenever external power comes or goes. */
  pbl_charger_set_listener(CHARGER, &s_listener);

  (void)prv_set_charging(charging);
  printf("charger: press Select to toggle charging\n");

  for (int i = 0; i < READS; i++) {
    ret = prv_read();
    if (ret != 0) {
      break;
    }

    for (int t = 0; t < READ_MS / POLL_MS; t++) {
      if (atomic_exchange(&s_toggle, false) && prv_set_charging(!charging) == 0) {
        charging = !charging;
      }
      pbl_sleep_ms(POLL_MS);
    }
  }

  (void)prv_set_charging(false);
  pbl_charger_set_listener(CHARGER, nullptr);

  if (ret != 0) {
    return ret;
  }

  printf("Sample done\n");

  return 0;
}
