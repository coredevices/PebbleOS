/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <pbl/kernel/msgq.h>
#include <pbl/kernel/time.h>
#include <pbl/usb/usbd.h>

#define RUN_S         60
#define WAKE_AFTER_MS 5000U

static const char *const s_names[] = {
  [PBL_USBD_EVENT_CONNECTED] = "connected", [PBL_USBD_EVENT_DISCONNECTED] = "disconnected",
  [PBL_USBD_EVENT_RESET] = "reset",         [PBL_USBD_EVENT_SUSPENDED] = "suspended",
  [PBL_USBD_EVENT_RESUMED] = "resumed",
};

/* The listener runs in the controller's interrupt: it queues, the main thread reports. */
static PBL_MSGQ_DEFINE(s_events, sizeof(uint8_t), 16);

static void prv_event(enum pbl_usbd_event evt) {
  uint8_t e = (uint8_t)evt;

  (void)pbl_msgq_put(&s_events, &e, PBL_NO_WAIT);
}

PBL_USBD_LISTENER_DEFINE(sample) = {
  .event = prv_event,
};

int main(void) {
  uint64_t end = pbl_uptime_ms() + RUN_S * 1000U;
  uint64_t suspended_at = 0U;
  bool suspended = false;
  uint8_t e;
  int ret;

  /* The serial port, the device's function, enabled it at boot: the events since are queued. */
  ret = pbl_usbd_enable();
  if (ret != 0) {
    fprintf(stderr, "usb: enable failed (%d)\n", ret);
    return ret;
  }

  while (pbl_uptime_ms() < end) {
    if (pbl_msgq_get(&s_events, &e, PBL_MSEC(100)) == 0) {
      printf("usb: %s\n", s_names[e]);
      suspended = e == PBL_USBD_EVENT_SUSPENDED;
      suspended_at = pbl_uptime_ms();
      continue;
    }

    if (suspended && pbl_uptime_ms() - suspended_at >= WAKE_AFTER_MS) {
      ret = pbl_usbd_wakeup();
      printf("usb: remote wakeup (%d)\n", ret);
      suspended = false;
    }
  }

  printf("Sample done\n");

  return 0;
}
