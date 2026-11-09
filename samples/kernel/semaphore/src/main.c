/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/timer.h>
#include <pbl/logging/logging.h>
#include <pbl/util/size.h>

#define NUM_WORKERS  4
#define NUM_CHANNELS 2

/* Binary: an interrupt tells a thread that what it waits for has happened. */
static PBL_SEM_DEFINE(s_transfer_done, 0, 1);

/* Counting: as many tokens as there are channels to share. */
static PBL_SEM_DEFINE(s_channels, NUM_CHANNELS, NUM_CHANNELS);

static PBL_MUTEX_DEFINE(s_stats_lock);
static unsigned int s_busy_channels;
static unsigned int s_max_busy_channels;

PBL_THREAD_STACK_DEFINE(s_stack_0, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_1, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_2, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_3, 1024);
static struct pbl_thread s_workers[NUM_WORKERS];

/* Runs in the tick interrupt, standing for the one of a peripheral. */
static void prv_transfer_isr(struct pbl_timer *timer) {
  pbl_sem_give(&s_transfer_done);
}

static struct pbl_timer s_transfer_timer = PBL_TIMER_INITIALIZER(prv_transfer_isr);

static int prv_transfer(uint32_t duration_ms) {
  pbl_timer_start(&s_transfer_timer, PBL_MSEC(duration_ms));

  if (pbl_sem_take(&s_transfer_done, PBL_MSEC(100)) != 0) {
    pbl_timer_stop(&s_transfer_timer);
    return -ETIMEDOUT;
  }

  return 0;
}

static void prv_worker(void *arg) {
  pbl_sem_take(&s_channels, PBL_FOREVER);

  pbl_mutex_lock(&s_stats_lock, PBL_FOREVER);
  s_busy_channels++;
  if (s_busy_channels > s_max_busy_channels) {
    s_max_busy_channels = s_busy_channels;
  }
  pbl_mutex_unlock(&s_stats_lock);

  pbl_sleep_ms(20);

  pbl_mutex_lock(&s_stats_lock, PBL_FOREVER);
  s_busy_channels--;
  pbl_mutex_unlock(&s_stats_lock);

  pbl_sem_give(&s_channels);
}

int main(void) {
  void *const stacks[NUM_WORKERS] = {s_stack_0, s_stack_1, s_stack_2, s_stack_3};
  int ret;

  ret = prv_transfer(10);
  PBL_LOG_INFO("main: 10 ms transfer returns %d", ret);

  ret = prv_transfer(5000);
  PBL_LOG_INFO("main: 5 s transfer returns %s", ret == -ETIMEDOUT ? "-ETIMEDOUT" : "other");

  for (unsigned int i = 0; i < NUM_WORKERS; i++) {
    const struct pbl_thread_attr attr = {
      .name = "worker",
      .entry = prv_worker,
      .prio = CONFIG_KERNEL_MAIN_THREAD_PRIO - 1,
      .privileged = true,
      .stack = stacks[i],
      .stack_size = sizeof(s_stack_0),
    };

    pbl_thread_create(&s_workers[i], &attr);
  }

  for (unsigned int i = 0; i < NUM_WORKERS; i++) {
    ret = PBL_POLL_SLEEP_MS(pbl_thread_state(&s_workers[i]) == PBL_THREAD_DEAD, 1, 1000);
    if (ret != 0) {
      PBL_LOG_ERR("main: worker %u did not end (%d)", i, ret);
      return ret;
    }
  }

  PBL_LOG_INFO("main: %u workers shared %u channels, at most %u busy at once", NUM_WORKERS,
               NUM_CHANNELS, s_max_busy_channels);
  PBL_LOG_INFO("main: %u channels free again", (unsigned int)pbl_sem_count(&s_channels));

  PBL_LOG_INFO("Sample done");

  return 0;
}
