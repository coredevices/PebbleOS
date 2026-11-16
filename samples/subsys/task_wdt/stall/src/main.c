/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/sem.h>
#include <pbl/task_wdt/task_wdt.h>

#define TIMEOUT_MS 1000U
#define STEP_MS    200U
#define STEPS      10U

/* What the main thread waits on when it gets stuck; only the watchdog callback gives it. */
static PBL_SEM_DEFINE(s_resource, 0, 1);
static PBL_SEM_DEFINE(s_recovered, 0, 1);
static PBL_SEM_DEFINE(s_work, 0, 1);

/* Runs on the watchdog thread on each check that finds the channel expired. */
static void *prv_expired(int channel_id, void *user_data) {
  printf("task_wdt: channel %d expired, freeing what main waits on\n", channel_id);
  pbl_sem_give(&s_resource);
  return nullptr;
}

/* Hooks the system can override: here they print, the defaults do nothing. */
void pbl_task_wdt_stalled(const struct pbl_task_wdt_stall *stall) {
  printf("task_wdt: stalled, channels in use 0x%02x, fed in time 0x%02x\n", stall->active_mask,
         stall->fed_mask);
}

void pbl_task_wdt_recovered(void) {
  printf("task_wdt: recovered\n");
  pbl_sem_give(&s_recovered);
}

int main(void) {
  int channel;

  channel = pbl_task_wdt_add(nullptr, TIMEOUT_MS, prv_expired, nullptr);
  if (channel < 0) {
    fprintf(stderr, "task_wdt: no channel (%d)\n", channel);
    return channel;
  }
  printf("task_wdt: main watched on channel %d, fed every %u ms at least\n", channel, TIMEOUT_MS);

  /* Busy work that feeds in time is never reported. */
  for (unsigned int i = 0; i < STEPS; i++) {
    pbl_sleep_ms(STEP_MS);
    pbl_task_wdt_feed(channel);
  }
  printf("task_wdt: %u ms of work fed in time\n", STEPS * STEP_MS);

  /* Waiting for work is no stall: the channel does not expire meanwhile. */
  pbl_task_wdt_set_waiting(true);
  (void)pbl_sem_take(&s_work, PBL_MSEC(3 * TIMEOUT_MS));
  pbl_task_wdt_set_waiting(false);
  printf("task_wdt: waited %u ms for work, not reported\n", 3 * TIMEOUT_MS);

  /* Stuck on something that never comes, until the callback frees it. */
  printf("task_wdt: main gets stuck\n");
  (void)pbl_sem_take(&s_resource, PBL_FOREVER);
  pbl_task_wdt_feed(channel);
  printf("task_wdt: main unblocked, fed again\n");

  (void)pbl_sem_take(&s_recovered, PBL_FOREVER);

  pbl_task_wdt_delete(channel);

  printf("Sample done\n");

  return 0;
}
