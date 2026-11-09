/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/delay.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/logging/logging.h>

#define NUM_WORKERS 3

/* A point no thread passes until all of them have reached it. */
struct barrier {
  struct pbl_mutex lock;
  struct pbl_sem gate;
  unsigned int parties;
  unsigned int waiting;
};

struct worker {
  unsigned int id;
  struct pbl_thread thread;
};

static struct barrier s_start_line = {
  .lock = PBL_MUTEX_INITIALIZER,
  .gate = PBL_SEM_INITIALIZER(0, NUM_WORKERS),
  .parties = NUM_WORKERS,
};

static struct worker s_workers[NUM_WORKERS];
static PBL_MUTEX_DEFINE(s_stats_lock);
static unsigned int s_ready;
static unsigned int s_passed;
static unsigned int s_passed_early;

PBL_THREAD_STACK_DEFINE(s_stack_0, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_1, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_2, 1024);

static void prv_barrier_wait(struct barrier *barrier) {
  pbl_mutex_lock(&barrier->lock, PBL_FOREVER);
  if (++barrier->waiting == barrier->parties) {
    /* The last one in lets the others through, and leaves the barrier ready for another round. */
    for (unsigned int i = 0; i < barrier->parties - 1; i++) {
      pbl_sem_give(&barrier->gate);
    }
    barrier->waiting = 0;
    pbl_mutex_unlock(&barrier->lock);
    return;
  }
  pbl_mutex_unlock(&barrier->lock);

  pbl_sem_take(&barrier->gate, PBL_FOREVER);
}

static void prv_worker(void *arg) {
  const struct worker *worker = arg;

  /* Getting ready takes each worker a different time. */
  pbl_sleep_ms(20 * (worker->id + 1));
  pbl_mutex_lock(&s_stats_lock, PBL_FOREVER);
  s_ready++;
  pbl_mutex_unlock(&s_stats_lock);
  PBL_LOG_INFO("worker %u: ready", worker->id);

  prv_barrier_wait(&s_start_line);

  pbl_mutex_lock(&s_stats_lock, PBL_FOREVER);
  if (s_ready != NUM_WORKERS) {
    s_passed_early++;
  }
  s_passed++;
  pbl_mutex_unlock(&s_stats_lock);
}

int main(void) {
  void *const stacks[NUM_WORKERS] = {s_stack_0, s_stack_1, s_stack_2};
  int ret;

  for (unsigned int i = 0; i < NUM_WORKERS; i++) {
    const struct pbl_thread_attr attr = {
      .name = "worker",
      .entry = prv_worker,
      .arg = &s_workers[i],
      .prio = CONFIG_KERNEL_MAIN_THREAD_PRIO - 1,
      .privileged = true,
      .stack = stacks[i],
      .stack_size = sizeof(s_stack_0),
    };

    s_workers[i].id = i;
    pbl_thread_create(&s_workers[i].thread, &attr);
  }

  for (unsigned int i = 0; i < NUM_WORKERS; i++) {
    ret = PBL_POLL_SLEEP_MS(pbl_thread_state(&s_workers[i].thread) == PBL_THREAD_DEAD, 1, 1000);
    if (ret != 0) {
      PBL_LOG_ERR("main: worker %u did not end (%d)", i, ret);
      return ret;
    }
  }

  PBL_LOG_INFO("main: %u past the barrier, %u of them before the last was ready", s_passed,
               s_passed_early);

  PBL_LOG_INFO("Sample done");

  return 0;
}
