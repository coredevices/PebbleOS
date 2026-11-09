/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdatomic.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/logging/logging.h>

#define NUM_PHILOSOPHERS 5
#define MEALS            3

struct philosopher {
  unsigned int seat;
  unsigned int meals;
  bool stuck;
  struct pbl_thread thread;
};

static struct pbl_mutex s_forks[NUM_PHILOSOPHERS];
static struct philosopher s_philosophers[NUM_PHILOSOPHERS];
static bool s_ordered;
static atomic_uint s_holding_one;

/* Stuck philosophers keep their fork until main has counted them. */
static PBL_SEM_DEFINE(s_give_up, 0, NUM_PHILOSOPHERS);

PBL_THREAD_STACK_DEFINE(s_stack_0, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_1, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_2, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_3, 1024);
PBL_THREAD_STACK_DEFINE(s_stack_4, 1024);

static void prv_philosopher(void *arg) {
  struct philosopher *philosopher = arg;
  struct pbl_mutex *first = &s_forks[philosopher->seat];
  struct pbl_mutex *second = &s_forks[(philosopher->seat + 1) % NUM_PHILOSOPHERS];

  /* The fix: everyone takes the lower numbered fork first, so no cycle of waiters can form. */
  if (s_ordered && first > second) {
    struct pbl_mutex *tmp = first;
    first = second;
    second = tmp;
  }

  while (philosopher->meals < MEALS) {
    pbl_mutex_lock(first, PBL_FOREVER);
    /*
     * A deadlock needs bad luck with the timing, which is staged here: nobody goes on until
     * every philosopher holds a fork. With the fix they cannot all hold one.
     */
    atomic_fetch_add(&s_holding_one, 1);
    do {
      pbl_thread_yield();
    } while (!s_ordered && atomic_load(&s_holding_one) < NUM_PHILOSOPHERS);

    /* The time limit stands in for the deadlock, which would otherwise end the sample here. */
    if (pbl_mutex_lock(second, PBL_MSEC(50)) != 0) {
      philosopher->stuck = true;
      pbl_sem_take(&s_give_up, PBL_FOREVER);
      pbl_mutex_unlock(first);
      return;
    }

    philosopher->meals++;
    pbl_sleep_ms(2);

    atomic_fetch_sub(&s_holding_one, 1);
    pbl_mutex_unlock(second);
    pbl_mutex_unlock(first);
  }
}

static int prv_dinner(bool ordered) {
  void *const stacks[NUM_PHILOSOPHERS] = {s_stack_0, s_stack_1, s_stack_2, s_stack_3, s_stack_4};
  unsigned int stuck = 0;
  unsigned int meals = 0;
  int ret;

  s_ordered = ordered;
  atomic_store(&s_holding_one, 0);

  for (unsigned int i = 0; i < NUM_PHILOSOPHERS; i++) {
    const struct pbl_thread_attr attr = {
      .name = "philosopher",
      .entry = prv_philosopher,
      .arg = &s_philosophers[i],
      .prio = CONFIG_KERNEL_MAIN_THREAD_PRIO - 1,
      .privileged = true,
      .stack = stacks[i],
      .stack_size = sizeof(s_stack_0),
    };

    s_philosophers[i].seat = i;
    s_philosophers[i].meals = 0;
    s_philosophers[i].stuck = false;
    pbl_thread_create(&s_philosophers[i].thread, &attr);
  }

  /* Long enough for every meal, or for every time limit to pass. */
  pbl_sleep_ms(200);

  for (unsigned int i = 0; i < NUM_PHILOSOPHERS; i++) {
    stuck += s_philosophers[i].stuck ? 1 : 0;
    meals += s_philosophers[i].meals;
    pbl_sem_give(&s_give_up);
  }

  for (unsigned int i = 0; i < NUM_PHILOSOPHERS; i++) {
    ret =
        PBL_POLL_SLEEP_MS(pbl_thread_state(&s_philosophers[i].thread) == PBL_THREAD_DEAD, 1, 1000);
    if (ret != 0) {
      PBL_LOG_ERR("dinner: philosopher %u did not leave (%d)", i, ret);
      return ret;
    }
  }
  pbl_sem_reset(&s_give_up);

  PBL_LOG_INFO("dinner: %s: %u meals eaten, %u of %u philosophers stuck with one fork",
               ordered ? "lower fork first" : "left fork first", meals, stuck, NUM_PHILOSOPHERS);

  return 0;
}

int main(void) {
  int ret;

  for (unsigned int i = 0; i < NUM_PHILOSOPHERS; i++) {
    pbl_mutex_init(&s_forks[i]);
  }

  ret = prv_dinner(false);
  if (ret != 0) {
    return ret;
  }

  ret = prv_dinner(true);
  if (ret != 0) {
    return ret;
  }

  PBL_LOG_INFO("Sample done");

  return 0;
}
