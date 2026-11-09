/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <inttypes.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/msgq.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/sem.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/time.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>

#define ITERATIONS 1000U

/* Runs body ITERATIONS times and reports the average and the best time of one. */
#define BENCH(name, body)                              \
  do {                                                 \
    uint32_t _total = 0;                               \
    uint32_t _best = UINT32_MAX;                       \
    for (unsigned int _i = 0; _i < ITERATIONS; _i++) { \
      uint32_t _start = pbl_cycle_get_32();            \
      body;                                            \
      uint32_t _cycles = pbl_cycle_get_32() - _start;  \
      _total += _cycles;                               \
      _best = _cycles < _best ? _cycles : _best;       \
    }                                                  \
    prv_report(name, _total / ITERATIONS, _best);      \
  } while (0)

static PBL_SEM_DEFINE(s_sem, 0, 1);
static PBL_MUTEX_DEFINE(s_mutex);
static PBL_MSGQ_DEFINE(s_msgq, sizeof(uint32_t), 4);

/* For the switches to a thread of higher priority and back. */
static PBL_SEM_DEFINE(s_ping, 0, 1);
static volatile uint32_t s_work_started;

PBL_THREAD_STACK_DEFINE(s_peer_stack, 1024);
PBL_THREAD_STACK_DEFINE(s_waiter_stack, 1024);
PBL_THREAD_STACK_DEFINE(s_short_stack, 1024);
static struct pbl_thread s_peer;
static struct pbl_thread s_waiter;
static struct pbl_thread s_short;

static void prv_report(const char *name, uint32_t average, uint32_t best) {
  uint32_t ns = (uint32_t)((uint64_t)average * 1000000000U / pbl_cycles_per_sec());

  /* No padding: hashed logs take plain conversions only. */
  PBL_LOG_INFO("bench: %s: avg %" PRIu32 " cycles (%" PRIu32 " ns), best %" PRIu32 " cycles", name,
               average, ns, best);
}

static void prv_peer(void *arg) {
  for (;;) {
    pbl_thread_yield();
  }
}

static void prv_waiter(void *arg) {
  for (;;) {
    pbl_sem_take(&s_ping, PBL_FOREVER);
  }
}

static void prv_short(void *arg) {
}

static void prv_work(struct pbl_work *work) {
  s_work_started = pbl_cycle_get_32();
}

static PBL_WORK_DEFINE(s_work, prv_work);

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
  uint32_t msg = 0;
  uint32_t start;

  start = pbl_cycle_get_32();
  pbl_sleep_ms(10);
  if (pbl_cycle_get_32() == start) {
    PBL_LOG_ERR("bench: the cycle counter does not count on this board");
    return -ENOTSUP;
  }

  PBL_LOG_INFO("bench: %u iterations each, %" PRIu32 " cycles per second", ITERATIONS,
               pbl_cycles_per_sec());

  BENCH("empty measurement", {});

  BENCH("sem give + take", {
    pbl_sem_give(&s_sem);
    pbl_sem_take(&s_sem, PBL_NO_WAIT);
  });

  BENCH("mutex lock + unlock", {
    pbl_mutex_lock(&s_mutex, PBL_NO_WAIT);
    pbl_mutex_unlock(&s_mutex);
  });

  BENCH("msgq put + get", {
    pbl_msgq_put(&s_msgq, &msg, PBL_NO_WAIT);
    pbl_msgq_get(&s_msgq, &msg, PBL_NO_WAIT);
  });

  BENCH("malloc + free, 64 bytes", { pbl_free(pbl_malloc(64)); });

  /* A thread of main's priority that only yields: each yield here is two switches. */
  prv_start(&s_peer, "peer", prv_peer, CONFIG_KERNEL_MAIN_THREAD_PRIO, s_peer_stack,
            sizeof(s_peer_stack));
  BENCH("yield, there and back", { pbl_thread_yield(); });
  pbl_thread_abort(&s_peer);

  /* A waiter above main: the give switches to it, and it blocks again at once. */
  prv_start(&s_waiter, "waiter", prv_waiter, CONFIG_KERNEL_MAIN_THREAD_PRIO + 1, s_waiter_stack,
            sizeof(s_waiter_stack));
  BENCH("sem wake, there and back", { pbl_sem_give(&s_ping); });
  pbl_thread_abort(&s_waiter);

  BENCH("thread create, run, end", {
    prv_start(&s_short, "short", prv_short, CONFIG_KERNEL_MAIN_THREAD_PRIO + 1, s_short_stack,
              sizeof(s_short_stack));
  });

  /* The high priority queue runs above main, so its work starts before submit returns. */
  {
    uint32_t total = 0;
    uint32_t best = UINT32_MAX;

    for (unsigned int i = 0; i < ITERATIONS; i++) {
      uint32_t submitted = pbl_cycle_get_32();
      pbl_work_submit(&pbl_sys_workq_high, &s_work);
      uint32_t cycles = s_work_started - submitted;

      total += cycles;
      best = cycles < best ? cycles : best;
    }
    prv_report("work submit to start", total / ITERATIONS, best);
  }

  PBL_LOG_INFO("Sample done");

  return 0;
}
