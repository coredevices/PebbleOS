/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/compiler.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/thread.h>
#include <pbl/kernel/types.h>
#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/mcu/fpu.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/services/sys_work.h>
#include <pbl/task_wdt/task_wdt.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <kernel/pebble_tasks.h>
#include <kernel/util/task_init.h>
#include <process_management/app_manager.h>

PBL_LOG_MODULE_DEFINE(service_sys_work, CONFIG_SERVICE_SYS_WORK_LOG_LEVEL);

#define APP_THROTTLE_TIME_MS 300

//! Apps get their own slots, so they wait for each other rather than starve the system.
#define SYS_WORK_POOL_SIZE          30
#define FROM_APP_SYS_WORK_POOL_SIZE 8
#define RAISED_POOL_SIZE            8
#define HIGH_POOL_SIZE              5

struct pbl_sys_work__heap_call {
  struct pbl_work work;
  SysWorkCallback cb;
  void *data;
};

struct pbl_sys_work__raised_call {
  SysWorkCallback cb;
  void *data;
};

static PBL_WORK_POOL_DEFINE(s_pool, SYS_WORK_POOL_SIZE);
static PBL_WORK_POOL_DEFINE(s_from_app_pool, FROM_APP_SYS_WORK_POOL_SIZE);
static PBL_WORK_POOL_DEFINE(s_raised_pool, RAISED_POOL_SIZE);
static PBL_WORK_POOL_DEFINE(s_high_pool, HIGH_POOL_SIZE);
static struct pbl_sys_work__raised_call s_raised_calls[RAISED_POOL_SIZE];
//! The callback behind a wrapper, which is all the queue knows about.
static SysWorkCallback s_wrapped_cb;
static bool s_initialized;

static int s_wdt_channel = -1;
static TimerID s_throttle_timer = TIMER_INVALID_ID;

static bool s_should_block_callbacks = false;
static uint32_t s_raised_priority_refcount;

static bool prv_is_accepting_callbacks() {
  return s_initialized && !s_should_block_callbacks;
}

static void prv_app_throttle_end(void *data) {
  struct pbl_thread *app = pebble_task_get_thread(PebbleTask_App);
  if (app) {
    pbl_thread_prio_set(app, CONFIG_APP_THREAD_PRIO);
  }
  PBL_LOG_DBG("Ending App Throttling");
}

static void prv_app_throttle_start(void) {
  static char s_last_throttled_app[PBL_THREAD_NAME_LEN];
  struct pbl_thread *app = pebble_task_get_thread(PebbleTask_App);
  if (!app) {
    return;
  }

  const char *name = pbl_thread_name(app);
  if (strcmp(s_last_throttled_app, name) != 0) {
    strcpy(s_last_throttled_app, name);
    PBL_LOG_WRN("Starting App Throttling for %s", name);
  } else {
    PBL_LOG_DBG("Starting App Throttling for %s", name);
  }

  pbl_thread_prio_set(app, PBL_PRIO_IDLE);
  new_timer_start(s_throttle_timer, APP_THROTTLE_TIME_MS, prv_app_throttle_end, nullptr, 0);
}

//! The system task is starved when it is ready to run but does not get the
//! CPU, or blocked in a callback on a lock the worker cannot release because
//! the app hogs the CPU. Parking the app briefly resolves both.
static void *prv_wdt_expired(int channel_id, void *user_data) {
  if (s_throttle_timer != TIMER_INVALID_ID &&
      (sys_work_is_ready_to_run() || sys_work_get_current_callback() != nullptr)) {
    prv_app_throttle_start();
  }
  return sys_work_get_current_callback();
}

static void prv_task_init(struct pbl_work *work) {
  task_init();
}

static PBL_WORK_DEFINE(s_task_init_work, prv_task_init);
static PBL_WORK_DEFINE(s_high_task_init_work, prv_task_init);

static void *prv_high_wdt_expired(int channel_id, void *user_data) {
  return pbl_workq_current_fn(&pbl_sys_workq_high);
}

static void prv_workq_wait(struct pbl_workq *q, bool waiting) {
  pbl_task_wdt_set_waiting(waiting);
}

static void prv_workq_done(struct pbl_workq *q) {
  mcu_fpu_cleanup();
  // Refresh the watchdog immediately, just in case the callback took a while to run.
  sys_work_watchdog_feed();
}

static const struct pbl_workq_ops s_workq_ops = {
  .wait = prv_workq_wait,
  .done = prv_workq_done,
};

static const struct pbl_workq_ops s_high_workq_ops = {
  .wait = prv_workq_wait,
};

void sys_work_init(void) {
  s_raised_priority_refcount = 0;
  s_initialized = true;

  struct pbl_thread *thread = pbl_workq_get_thread(&pbl_sys_workq);

  pebble_task_register(PebbleTask_KernelBackground, thread);
  s_wdt_channel = pbl_task_wdt_add(thread, CONFIG_TASK_WDT_TIMEOUT_MS, prv_wdt_expired, nullptr);
  PBL_ASSERTN(s_wdt_channel >= 0);
  pbl_workq_set_ops(&pbl_sys_workq, &s_workq_ops);
  pbl_work_submit(&pbl_sys_workq, &s_task_init_work);

  thread = pbl_workq_get_thread(&pbl_sys_workq_high);
  pebble_task_register(PebbleTask_NewTimers, thread);
  PBL_ASSERTN(pbl_task_wdt_add(thread, CONFIG_TASK_WDT_TIMEOUT_MS, prv_high_wdt_expired, nullptr) >=
              0);
  pbl_workq_set_ops(&pbl_sys_workq_high, &s_high_workq_ops);
  pbl_work_submit(&pbl_sys_workq_high, &s_high_task_init_work);
}

void sys_work_timer_init(void) {
  s_throttle_timer = new_timer_create();
}

void sys_work_watchdog_feed(void) {
  pbl_task_wdt_feed(s_wdt_channel);
}

[[noreturn]] static void handle_sys_work_send_failure(SysWorkCallback cb, uintptr_t caller_lr) {
  void *current_cb = sys_work_get_current_callback();

  PBL_LOG_ERR("System task queue full. Dropped cb: %p, current cb: %p", cb, current_cb);

  RebootReason reason = {
    .code = RebootReasonCode_EventQueueFull,
    .event_queue = {
      .push_lr = (uint32_t)caller_lr,
      .current_event = (uint32_t)(uintptr_t)current_cb,
      .dropped_event = (uint32_t)(uintptr_t)cb
    }
  };
  reboot_reason_set(&reason);

  reset_due_to_software_failure();
}

static int prv_call(struct pbl_work_pool *pool, SysWorkCallback cb, void *data,
                    pbl_timeout_t timeout) {
  return pbl_work_call(&pbl_sys_workq, pool, cb, data, timeout);
}

static void prv_heap_call_fn(struct pbl_work *work) {
  struct pbl_sys_work__heap_call *call = container_of(work, struct pbl_sys_work__heap_call, work);
  SysWorkCallback cb = call->cb;
  void *data = call->data;

  pbl_free(call);
  s_wrapped_cb = cb;
  cb(data);
  s_wrapped_cb = nullptr;
}

//! The system task runs its own calls, so it must never wait for a slot: calls that do not fit
//! go on the heap instead.
static void prv_add_from_sys_work(SysWorkCallback cb, void *data, uintptr_t caller_lr) {
  if (prv_call(&s_pool, cb, data, PBL_NO_WAIT) == 0) {
    return;
  }

  struct pbl_sys_work__heap_call *call = pbl_malloc(sizeof(*call));
  if (!call) {
    handle_sys_work_send_failure(cb, caller_lr);
  }

  *call = (struct pbl_sys_work__heap_call){.cb = cb, .data = data};
  pbl_work_init(&call->work, prv_heap_call_fn);
  pbl_work_submit(&pbl_sys_workq, &call->work);
}

bool sys_work_add_from_isr(SysWorkCallback cb, void *data) {
  // Capture caller LR at entry; reading from a deeper helper is unreliable.
  uintptr_t caller_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);
  if (!prv_is_accepting_callbacks()) {
    return false;
  }

  if (prv_call(&s_pool, cb, data, PBL_NO_WAIT) != 0) {
    handle_sys_work_send_failure(cb, caller_lr);
  }

  return true;
}

bool sys_work_add_high(SysWorkCallback cb, void *data) {
  return pbl_work_call(&pbl_sys_workq_high, &s_high_pool, cb, data, PBL_TICKS(50)) == 0;
}

void sys_work_add_high_from_isr(SysWorkCallback cb, void *data) {
  (void)pbl_work_call(&pbl_sys_workq_high, &s_high_pool, cb, data, PBL_NO_WAIT);
}

bool sys_work_try_add(SysWorkCallback cb, void *data) {
  if (!prv_is_accepting_callbacks()) {
    return false;
  }

  return prv_call(&s_pool, cb, data, PBL_NO_WAIT) == 0;
}

static void prv_raised_call_fn(void *data) {
  struct pbl_sys_work__raised_call *call = data;
  SysWorkCallback cb = call->cb;
  void *cb_data = call->data;

  pbl_irq_lock();
  call->cb = nullptr;
  pbl_irq_unlock();

  s_wrapped_cb = cb;
  cb(cb_data);
  s_wrapped_cb = nullptr;
  mcu_fpu_cleanup();
  sys_work_enable_raised_priority(false);
}

bool sys_work_try_add_raised(SysWorkCallback cb, void *data) {
  if (!prv_is_accepting_callbacks()) {
    return false;
  }

  bool success = false;

  // The queued call owns the boost until its callback returns, even if the device stops in the
  // meantime. Rejected work must not retain a reference.
  pbl_irq_lock();
  for (size_t i = 0; i < ARRAY_LENGTH(s_raised_calls); i++) {
    struct pbl_sys_work__raised_call *call = &s_raised_calls[i];
    if (call->cb == nullptr) {
      if (pbl_work_call(&pbl_sys_workq, &s_raised_pool, prv_raised_call_fn, call, PBL_NO_WAIT) ==
          0) {
        *call = (struct pbl_sys_work__raised_call){.cb = cb, .data = data};
        sys_work_enable_raised_priority(true);
        success = true;
      }
      break;
    }
  }
  pbl_irq_unlock();
  return success;
}

bool sys_work_add(SysWorkCallback cb, void *data) {
  uintptr_t caller_lr = (uintptr_t)PBL_RETURN_ADDRESS(0);
  if (!prv_is_accepting_callbacks()) {
    return false;
  }

  const PebbleTask task = pebble_task_get_current();
  if (task == PebbleTask_App) {
    // If we're the app and we've filled up our slots, the app just gets to wait.
    // FIXME: In the future when we want to bound the amount of time a syscall can take this will
    // have to change.
    prv_call(&s_from_app_pool, cb, data, PBL_FOREVER);
  } else if (task == PebbleTask_KernelBackground) {
    prv_add_from_sys_work(cb, data, caller_lr);
  } else if (prv_call(&s_pool, cb, data, PBL_MSEC(3000)) != 0) {
    // Back ourselves up and wait a reasonable amount of time before failing. If the queue is really
    // backed up we want to fail here and not just get killed by the watchdog.
    handle_sys_work_send_failure(cb, caller_lr);
  }

  return true;
}

void sys_work_block(bool block) {
  s_should_block_callbacks = block;
}

uint32_t sys_work_get_available_space(void) {
  const bool is_app = pebble_task_get_current() == PebbleTask_App;
  return pbl_work_pool_num_free(is_app ? &s_from_app_pool : &s_pool);
}

void *sys_work_get_current_callback(void) {
  return s_wrapped_cb ? (void *)s_wrapped_cb : pbl_workq_current_fn(&pbl_sys_workq);
}

void sys_work_enable_raised_priority(bool is_raised) {
  pbl_irq_lock();
  if (is_raised) {
    PBL_ASSERTN(s_raised_priority_refcount < UINT32_MAX);
    if (s_raised_priority_refcount == UINT32_MAX) {
      pbl_irq_unlock();
      return;
    }
    if (s_raised_priority_refcount++ == 0) {
      pbl_thread_prio_set(pebble_task_get_thread(PebbleTask_KernelBackground),
                          CONFIG_SERVICE_SYS_WORK_THREAD_RAISED_PRIO);
    }
  } else {
    PBL_ASSERTN(s_raised_priority_refcount > 0);
    if (s_raised_priority_refcount == 0) {
      pbl_irq_unlock();
      return;
    }
    if (--s_raised_priority_refcount == 0) {
      pbl_thread_prio_set(pebble_task_get_thread(PebbleTask_KernelBackground),
                          CONFIG_KERNEL_SYS_WORKQ_PRIO);
    }
  }
  pbl_irq_unlock();
}

bool sys_work_is_ready_to_run(void) {
  // check if system task is ready to go (instead of e.g. waiting for a mutex)
  return pbl_thread_state(pbl_workq_get_thread(&pbl_sys_workq)) == PBL_THREAD_READY;
}

#if UNITTEST
void sys_work_reset_for_test(void) {
  static struct pbl_work_call s_slots[SYS_WORK_POOL_SIZE];
  static struct pbl_work_call s_from_app_slots[FROM_APP_SYS_WORK_POOL_SIZE];
  static struct pbl_work_call s_raised_slots[RAISED_POOL_SIZE];
  static struct pbl_work_call s_high_slots[HIGH_POOL_SIZE];

  pbl_work_pool_init(&s_pool, s_slots, SYS_WORK_POOL_SIZE);
  pbl_work_pool_init(&s_from_app_pool, s_from_app_slots, FROM_APP_SYS_WORK_POOL_SIZE);
  pbl_work_pool_init(&s_raised_pool, s_raised_slots, RAISED_POOL_SIZE);
  pbl_work_pool_init(&s_high_pool, s_high_slots, HIGH_POOL_SIZE);
  memset(s_raised_calls, 0, sizeof(s_raised_calls));
  s_wrapped_cb = nullptr;
  s_initialized = false;
}
#endif
