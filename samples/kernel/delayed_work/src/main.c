/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/delay.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>

#define DEBOUNCE_MS  200
#define HEARTBEAT_MS 20

static unsigned int s_presses;
static unsigned int s_heartbeats;

static void prv_button_settled(struct pbl_work *work) {
  s_presses++;
  PBL_LOG_INFO("button: settled, press %u", s_presses);
}

static void prv_heartbeat(struct pbl_work *work);

static PBL_DELAYED_WORK_DEFINE(s_debounce, prv_button_settled);
static PBL_DELAYED_WORK_DEFINE(s_heartbeat, prv_heartbeat);

/* Unlike a timer callback this runs on a thread, and may block or take locks. */
static void prv_heartbeat(struct pbl_work *work) {
  s_heartbeats++;
  /* Counts from the last expiry, not from now, so the period does not drift. */
  pbl_delayed_work_schedule_next(&s_heartbeat, pbl_ms_to_ticks(HEARTBEAT_MS));
}

/* What a button interrupt would do on every edge, bounces included. */
static void prv_button_edge(void) {
  pbl_delayed_work_reschedule(&pbl_sys_workq, &s_debounce, PBL_MSEC(DEBOUNCE_MS));
}

int main(void) {
  bool kept;

  PBL_LOG_INFO("main: five edges 5 ms apart, each pushing the work back");
  for (unsigned int i = 0; i < 5; i++) {
    prv_button_edge();
    pbl_sleep_ms(5);
  }
  pbl_sleep_ms(2 * DEBOUNCE_MS);

  /* schedule() keeps a delay that is running, where reschedule() replaces it. */
  pbl_delayed_work_schedule(&pbl_sys_workq, &s_debounce, PBL_MSEC(DEBOUNCE_MS));
  kept = !pbl_delayed_work_schedule(&pbl_sys_workq, &s_debounce, PBL_SEC(10));
  PBL_LOG_INFO("main: scheduled twice, first delay kept: %s", kept ? "yes" : "no");
  pbl_sleep_ms(2 * DEBOUNCE_MS);

  pbl_delayed_work_schedule(&pbl_sys_workq, &s_heartbeat, PBL_MSEC(HEARTBEAT_MS));
  pbl_sleep_ms(5 * HEARTBEAT_MS + HEARTBEAT_MS / 2);

  /* Stops it for good, even though its function schedules it again. */
  pbl_delayed_work_cancel_sync(&s_heartbeat);
  PBL_LOG_INFO("main: heartbeat stopped after %u beats, pending: %s", s_heartbeats,
               pbl_delayed_work_is_pending(&s_heartbeat) ? "yes" : "no");

  PBL_LOG_INFO("Sample done");

  return 0;
}
