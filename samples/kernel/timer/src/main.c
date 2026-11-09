/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/timer.h>
#include <pbl/logging/logging.h>
#include <pbl/util/misc.h>

#define BLINKS          5
#define BLINK_PERIOD_MS 20

/* A timer lives in the object it acts on, which its callback gets back. */
struct blinker {
  struct pbl_timer timer;
  struct pbl_sem done;
  unsigned int blinks;
  uint64_t last_ms;
};

/* Callbacks run in the tick interrupt: keep them short, and never block. */
static void prv_blink(struct pbl_timer *timer) {
  struct blinker *blinker = container_of(timer, struct blinker, timer);

  blinker->blinks++;
  blinker->last_ms = pbl_uptime_ms();

  /* Timers are one-shot: one that repeats starts itself again. */
  if (blinker->blinks < BLINKS) {
    pbl_timer_start(timer, PBL_MSEC(BLINK_PERIOD_MS));
  } else {
    pbl_sem_give(&blinker->done);
  }
}

static struct blinker s_blinker = {
  .timer = PBL_TIMER_INITIALIZER(prv_blink),
  .done = PBL_SEM_INITIALIZER(0, 1),
};

int main(void) {
  uint64_t start_ms = pbl_uptime_ms();
  int ret;

  pbl_timer_start(&s_blinker.timer, PBL_MSEC(BLINK_PERIOD_MS));

  ret = pbl_sem_take(&s_blinker.done, PBL_SEC(1));
  if (ret != 0) {
    PBL_LOG_ERR("main: timer did not expire (%d)", ret);
    return ret;
  }

  PBL_LOG_INFO("main: %u blinks, the last one %s %u ms after the start", s_blinker.blinks,
               s_blinker.last_ms - start_ms >= BLINKS * BLINK_PERIOD_MS ? "at least" : "less than",
               BLINKS * BLINK_PERIOD_MS);

  s_blinker.blinks = 0;
  pbl_timer_start(&s_blinker.timer, PBL_SEC(10));
  pbl_sleep_ms(10);
  PBL_LOG_INFO("main: started for 10 s, active: %s, more than 5 s left: %s",
               pbl_timer_is_active(&s_blinker.timer) ? "yes" : "no",
               pbl_ticks_to_ms(pbl_timer_remaining(&s_blinker.timer)) > 5000 ? "yes" : "no");

  pbl_timer_stop(&s_blinker.timer);
  PBL_LOG_INFO("main: stopped, active: %s", pbl_timer_is_active(&s_blinker.timer) ? "yes" : "no");
  pbl_sleep_ms(20);
  PBL_LOG_INFO("main: stopped before it expired, %u blinks since", s_blinker.blinks);

  PBL_LOG_INFO("Sample done");

  return 0;
}
