/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>

#include <pbl/drivers/battery.h>
#include <pbl/kernel/delay.h>

#define READS 3

static const char *const s_status[] = {
  [BatteryChargeStatusUnknown] = "unknown",     [BatteryChargeStatusComplete] = "complete",
  [BatteryChargeStatusTrickle] = "trickle",     [BatteryChargeStatusCC] = "constant current",
  [BatteryChargeStatusCV] = "constant voltage",
};

static void prv_usb_changed(bool is_connected) {
  printf("battery: USB power %s\n", is_connected ? "connected" : "removed");
}

int main(void) {
  battery_init();

  /* Called from a work queue whenever USB power comes or goes. */
  battery_set_connection_handler(prv_usb_changed);

  for (int i = 0; i < READS; i++) {
    BatteryConstants c;
    BatteryChargeStatus status;
    int ret;

    ret = battery_get_constants(&c);
    if (ret != 0) {
      fprintf(stderr, "battery: measurement failed (%d)\n", ret);
      return ret;
    }

    ret = battery_charge_status_get(&status);
    if (ret != 0) {
      fprintf(stderr, "battery: charge status failed (%d)\n", ret);
      return ret;
    }

    printf("battery: %d mV, %d uA, %d mC\n", (int)c.v_mv, (int)c.i_ua, (int)c.t_mc);
    printf("battery: USB %s, %s, charge %s\n",
           battery_is_usb_connected() ? "connected" : "disconnected",
           battery_charge_controller_thinks_we_are_charging() ? "charging" : "not charging",
           s_status[status]);

    pbl_sleep_ms(1000);
  }

  battery_set_connection_handler(nullptr);

  printf("Sample done\n");

  return 0;
}
