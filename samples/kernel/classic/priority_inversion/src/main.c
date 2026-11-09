/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>

/* main() is the high priority thread. */
#define MEDIUM_PRIO (CONFIG_KERNEL_MAIN_THREAD_PRIO - 1)
#define LOW_PRIO    (CONFIG_KERNEL_MAIN_THREAD_PRIO - 2)

static PBL_MUTEX_DEFINE(s_bus_lock);
static volatile bool s_high_waits;
static volatile bool s_medium_done;

PBL_THREAD_STACK_DEFINE(s_low_stack, 1024);
PBL_THREAD_STACK_DEFINE(s_medium_stack, 1024);
static const struct pbl_thread *s_high;
static struct pbl_thread s_low;
static struct pbl_thread s_medium;

static void prv_use_cpu(uint32_t ms) {
  uint64_t start = pbl_uptime_ms();

  while (pbl_uptime_ms() - start < ms) {
  }
}

static void prv_low(void *arg) {
  const struct pbl_thread *self = pbl_thread_current();

  pbl_mutex_lock(&s_bus_lock, PBL_FOREVER);
  /* A long piece of work, which high comes in the middle of. */
  while (!s_high_waits) {
  }
  prv_use_cpu(10);
  /* While high waits for the lock, its holder runs at high's priority. */
  PBL_LOG_INFO("low: ends its work at the priority of high: %s",
               self->prio == s_high->prio ? "yes" : "no");
  pbl_mutex_unlock(&s_bus_lock);

  PBL_LOG_INFO("low: back to its own priority: %s",
               self->prio == pbl_thread_prio_get(self) ? "yes" : "no");
}

/* Needs no lock, only the CPU, and for longer than high can wait. */
static void prv_medium(void *arg) {
  prv_use_cpu(100);
  s_medium_done = true;
  PBL_LOG_INFO("medium: done");
}

static void prv_start(struct pbl_thread *t, const char *name, pbl_thread_entry_t entry,
                      pbl_prio_t prio, void *stack, size_t stack_size) {
  const struct pbl_thread_attr attr = {
    .name = name,
    .entry = entry,
    .prio = prio,
    .privileged = true,
    .stack = stack,
    .stack_size = stack_size,
  };

  pbl_thread_create(t, &attr);
}

int main(void) {
  int ret;

  s_high = pbl_thread_current();
  prv_start(&s_low, "low", prv_low, LOW_PRIO, s_low_stack, sizeof(s_low_stack));
  /* Lets low take the lock. */
  pbl_sleep_ms(5);

  /* Ready to run, and above low: without inheritance it would run first, for 100 ms. */
  prv_start(&s_medium, "medium", prv_medium, MEDIUM_PRIO, s_medium_stack, sizeof(s_medium_stack));

  s_high_waits = true;
  ret = pbl_mutex_lock(&s_bus_lock, PBL_FOREVER);
  if (ret != 0) {
    PBL_LOG_ERR("high: cannot take the lock (%d)", ret);
    return ret;
  }
  PBL_LOG_INFO("high: got the lock %s medium had the CPU", s_medium_done ? "after" : "before");
  pbl_mutex_unlock(&s_bus_lock);

  ret = PBL_POLL_SLEEP_MS(pbl_thread_state(&s_low) == PBL_THREAD_DEAD, 5, 1000);
  if (ret != 0) {
    PBL_LOG_ERR("high: low did not end (%d)", ret);
    return ret;
  }

  PBL_LOG_INFO("Sample done");

  return 0;
}
