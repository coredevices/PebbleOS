/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/task_wdt/task_wdt.h>

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <pbl/drivers/watchdog.h>
#include <pbl/logging/logging.h>

#include "pbl/kernel/debug.h"
#include "pbl/kernel/irq.h"
#include "pbl/kernel/sched.h"
#include "pbl/kernel/thread.h"
#include "pbl/kernel/types.h"
#include "system/die.h"
#include "system/passert.h"
#include "system/reboot_reason.h"

PBL_LOG_MODULE_DEFINE(task_wdt, CONFIG_TASK_WDT_LOG_LEVEL);

#define NUM_CHANNELS CONFIG_TASK_WDT_CHANNELS
_Static_assert(NUM_CHANNELS <= 8, "the reboot reason records the channels as 8-bit masks");

struct channel {
  struct pbl_thread *thread;
  pbl_tick_t timeout;
  pbl_tick_t deadline;
  pbl_task_wdt_callback_t callback;
  void *user_data;
  bool active;
};

struct expired {
  int id;
  struct pbl_thread *thread;
  uint32_t since_feed_ms;
  uint32_t overdue_ms;
  pbl_task_wdt_callback_t callback;
  void *user_data;
};

static struct channel s_channels[NUM_CHANNELS];
static bool s_suspended;
static bool s_suspend_timed;
static pbl_tick_t s_suspend_until;
static bool s_stalled;

static struct pbl_thread s_thread;
PBL_THREAD_STACK_DEFINE(s_stack, CONFIG_TASK_WDT_THREAD_STACK_SIZE);

static inline bool prv_reached(pbl_tick_t now, pbl_tick_t when) {
  return (int32_t)(now - when) >= 0;
}

static void prv_feed_locked(struct channel *ch, pbl_tick_t now) {
  ch->deadline = now + ch->timeout;
}

static void prv_feed_all_locked(pbl_tick_t now) {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    if (s_channels[i].active) {
      prv_feed_locked(&s_channels[i], now);
    }
  }
}

static bool prv_valid(int channel_id) {
  return channel_id >= 0 && channel_id < NUM_CHANNELS && s_channels[channel_id].active;
}

static size_t prv_collect_expired(struct expired *expired, uint8_t *fed_mask,
                                  uint8_t *active_mask) {
  size_t num_expired = 0;
  pbl_tick_t now = pbl_uptime_ticks();

  *fed_mask = 0;
  *active_mask = 0;

  pbl_irq_lock();
  if (s_suspended && s_suspend_timed && prv_reached(now, s_suspend_until)) {
    s_suspended = false;
  }
  if (s_suspended) {
    prv_feed_all_locked(now);
  }
  for (int i = 0; i < NUM_CHANNELS; i++) {
    const struct channel *ch = &s_channels[i];
    if (!ch->active) {
      continue;
    }
    *active_mask |= 1u << i;
    if (!prv_reached(now, ch->deadline)) {
      *fed_mask |= 1u << i;
      continue;
    }
    pbl_tick_t overdue = now - ch->deadline;
    expired[num_expired++] = (struct expired){
      .id = i,
      .thread = ch->thread,
      .since_feed_ms = pbl_ticks_to_ms(ch->timeout + overdue),
      .overdue_ms = pbl_ticks_to_ms(overdue),
      .callback = ch->callback,
      .user_data = ch->user_data,
    };
  }
  pbl_irq_unlock();

  return num_expired;
}

static bool prv_reboot_reason_is_ours(void) {
  RebootReason current;
  reboot_reason_get(&current);
  return current.code == RebootReasonCode_Unknown || current.code == RebootReasonCode_Watchdog;
}

//! Logs every expired channel, gives each callback a chance to recover the
//! thread and records the highest-priority stuck thread, which is the most
//! likely culprit when several are stuck, in the reboot reason.
//! @return the longest time any channel has been expired for.
static uint32_t prv_report(const struct expired *expired, size_t num_expired, uint8_t fed_mask,
                           uint8_t active_mask) {
  RebootReason reason = {
    .code = RebootReasonCode_Watchdog,
    .data8 = {fed_mask, active_mask},
  };
  pbl_prio_t worst_prio = 0;
  bool have_worst = false;
  uint32_t max_overdue_ms = 0;

  PBL_LOG_SYNC_WRN("Task watchdog: channels fed 0x%" PRIx8 " active 0x%" PRIx8, fed_mask,
                   active_mask);

  for (size_t i = 0; i < num_expired; i++) {
    const struct expired *e = &expired[i];
    struct pbl_thread_saved_regs regs;

    pbl_thread_saved_regs(e->thread, &regs);
    PBL_LOG_SYNC_WRN("<%s> not fed for %" PRIu32 " ms: PC %p LR %p", pbl_thread_name(e->thread),
                     e->since_feed_ms, (void *)regs.pc, (void *)regs.lr);

    void *work = e->callback ? e->callback(e->id, e->user_data) : NULL;
    if (work) {
      PBL_LOG_SYNC_WRN("<%s> running %p", pbl_thread_name(e->thread), work);
    }

    pbl_prio_t prio = pbl_thread_prio_get(e->thread);
    if (!have_worst || prio > worst_prio) {
      have_worst = true;
      worst_prio = prio;
      reason.watchdog.stuck_task_pc = regs.pc;
      reason.watchdog.stuck_task_lr = regs.lr;
      reason.watchdog.stuck_task_callback = (uint32_t)(uintptr_t)work;
    }
    if (e->overdue_ms > max_overdue_ms) {
      max_overdue_ms = e->overdue_ms;
    }
  }

  if (prv_reboot_reason_is_ours()) {
    reboot_reason_clear();
    reboot_reason_set(&reason);
  }

  return max_overdue_ms;
}

static void prv_check(void) {
  struct expired expired[NUM_CHANNELS];
  uint8_t fed_mask;
  uint8_t active_mask;

  size_t num_expired = prv_collect_expired(expired, &fed_mask, &active_mask);
  if (num_expired == 0) {
    if (s_stalled) {
      s_stalled = false;
      if (prv_reboot_reason_is_ours()) {
        reboot_reason_clear();
      }
      PBL_LOG_SYNC_WRN("Task watchdog: recovered from a stall");
    }
    watchdog_feed();
    return;
  }

  s_stalled = true;
  uint32_t overdue_ms = prv_report(expired, num_expired, fed_mask, active_mask);
  if (overdue_ms >= CONFIG_TASK_WDT_GRACE_MS) {
#ifdef CONFIG_WATCHDOG
    // The orderly teardown system_reset() performs from thread context could
    // block on the stuck thread and lose the core dump; a locked-out
    // scheduler makes it skip straight to the dump.
    pbl_irq_lock();
    reset_due_to_software_failure();
#else
    PBL_LOG_SYNC_ERR("Task watchdog: expired, not resetting (CONFIG_WATCHDOG=n)");
#endif
  }
  watchdog_feed();
}

static void prv_thread_entry(void *arg) {
  for (;;) {
    pbl_thread_sleep(PBL_MSEC(CONFIG_TASK_WDT_CHECK_PERIOD_MS));
    prv_check();
  }
}

void pbl_task_wdt_init(void) {
  pbl_irq_lock();
  prv_feed_all_locked(pbl_uptime_ticks());
  pbl_irq_unlock();

  struct pbl_thread_attr attr = {
    .name = "TaskWDT",
    .entry = prv_thread_entry,
    .prio = PBL_PRIO_MAX,
    .privileged = true,
    .stack = s_stack,
    .stack_size = sizeof(s_stack),
  };
  PBL_ASSERTN(pbl_thread_create(&s_thread, &attr) == 0);
}

int pbl_task_wdt_add(struct pbl_thread *thread, uint32_t timeout_ms,
                     pbl_task_wdt_callback_t callback, void *user_data) {
  int id = -ENOMEM;

  if (!thread) {
    thread = pbl_thread_current();
  }

  pbl_irq_lock();
  for (int i = 0; i < NUM_CHANNELS; i++) {
    struct channel *ch = &s_channels[i];
    if (ch->active) {
      continue;
    }
    *ch = (struct channel){
      .thread = thread,
      .timeout = pbl_ms_to_ticks(timeout_ms),
      .callback = callback,
      .user_data = user_data,
      .active = true,
    };
    prv_feed_locked(ch, pbl_uptime_ticks());
    id = i;
    break;
  }
  pbl_irq_unlock();

  return id;
}

int pbl_task_wdt_delete(int channel_id) {
  int rc = 0;

  pbl_irq_lock();
  if (prv_valid(channel_id)) {
    s_channels[channel_id].active = false;
  } else {
    rc = -EINVAL;
  }
  pbl_irq_unlock();

  return rc;
}

int pbl_task_wdt_feed(int channel_id) {
  int rc = 0;

  pbl_irq_lock();
  if (prv_valid(channel_id)) {
    prv_feed_locked(&s_channels[channel_id], pbl_uptime_ticks());
  } else {
    rc = -EINVAL;
  }
  pbl_irq_unlock();

  return rc;
}

void pbl_task_wdt_feed_thread(struct pbl_thread *thread) {
  if (!thread) {
    return;
  }

  pbl_irq_lock();
  pbl_tick_t now = pbl_uptime_ticks();
  for (int i = 0; i < NUM_CHANNELS; i++) {
    struct channel *ch = &s_channels[i];
    if (ch->active && ch->thread == thread) {
      prv_feed_locked(ch, now);
    }
  }
  pbl_irq_unlock();
}

void pbl_task_wdt_feed_self(void) {
  pbl_task_wdt_feed_thread(pbl_thread_current());
}

void pbl_task_wdt_feed_all(void) {
  pbl_irq_lock();
  prv_feed_all_locked(pbl_uptime_ticks());
  pbl_irq_unlock();
}

void pbl_task_wdt_suspend(uint32_t timeout_ms) {
  pbl_irq_lock();
  pbl_tick_t now = pbl_uptime_ticks();
  s_suspended = true;
  s_suspend_timed = timeout_ms != 0;
  s_suspend_until = now + pbl_ms_to_ticks(timeout_ms);
  prv_feed_all_locked(now);
  pbl_irq_unlock();
}

void pbl_task_wdt_resume(void) {
  pbl_irq_lock();
  s_suspended = false;
  prv_feed_all_locked(pbl_uptime_ticks());
  pbl_irq_unlock();
}

#if UNITTEST
void pbl_task_wdt_reset_for_test(void) {
  memset(s_channels, 0, sizeof(s_channels));
  memset(&s_thread, 0, sizeof(s_thread));
  s_suspended = false;
  s_suspend_timed = false;
  s_suspend_until = 0;
  s_stalled = false;
}
#endif
