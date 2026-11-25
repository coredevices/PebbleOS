/* SPDX-FileCopyrightText: 2025 Google LLC */
/* SPDX-FileCopyrightText: 2015-2024 The Apache Software Foundation */
/* SPDX-License-Identifier: Apache-2.0 */

// This is derived from the freertos port provided by NimBLE
// and modified to suit Pebble OS (timers, mutexes).

#include "nimble/nimble_npl.h"
#include "nimble/nimble_port.h"
#include "pbl/kernel/irq.h"
#include "pbl/kernel/mutex.h"
#include "pbl/kernel/types.h"
#include "pbl/kernel/work.h"
#include "pbl/mcu/interrupts.h"
#include "pbl/util/math.h"
#include "pbl/util/misc.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <pbl/kernel/assert.h>
#include <pbl/kernel/fatal.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>

struct ble_npl_event *npl_pebble_eventq_get(struct ble_npl_eventq *evq, ble_npl_time_t tmo) {
  struct ble_npl_event *ev = NULL;

  if (mcu_state_is_isr()) {
    assert(tmo == 0);
  }

  if (pbl_msgq_get(&evq->q, &ev, PBL_TICKS(tmo)) != 0) {
    return NULL;
  }

  ev->queued = false;
  return ev;
}

void npl_pebble_eventq_put(struct ble_npl_eventq *evq, struct ble_npl_event *ev) {
  if (ev->queued) {
    return;
  }

  ev->queued = true;

  const bool no_wait = mcu_state_is_isr() || pbl_irq_is_locked();
  int rc = pbl_msgq_put(&evq->q, &ev, no_wait ? PBL_NO_WAIT : PBL_FOREVER);
  assert(rc == 0);
}

void npl_pebble_eventq_remove(struct ble_npl_eventq *evq, struct ble_npl_event *ev) {
  struct ble_npl_event *tmp_ev;

  if (!ev->queued) {
    return;
  }

  // The queue cannot remove an arbitrary element, so drain it and put back
  // everything but the one being removed.
  const bool in_isr = mcu_state_is_isr();
  if (!in_isr) {
    pbl_irq_lock();
  }

  uint32_t count = pbl_msgq_num_used(&evq->q);
  for (uint32_t i = 0; i < count; i++) {
    int rc = pbl_msgq_get(&evq->q, &tmp_ev, PBL_NO_WAIT);
    assert(rc == 0);

    if (tmp_ev == ev) {
      continue;
    }

    rc = pbl_msgq_put(&evq->q, &tmp_ev, PBL_NO_WAIT);
    assert(rc == 0);
  }

  if (!in_isr) {
    pbl_irq_unlock();
  }

  ev->queued = 0;
}

ble_npl_error_t npl_pebble_mutex_init(struct ble_npl_mutex *mu) {
  if (!mu) {
    return BLE_NPL_INVALID_PARAM;
  }

  pbl_mutex_init(&mu->handle);

  return BLE_NPL_OK;
}

ble_npl_error_t npl_pebble_mutex_pend(struct ble_npl_mutex *mu, ble_npl_time_t timeout) {
  if (!mu) {
    return BLE_NPL_INVALID_PARAM;
  }

  if (mcu_state_is_isr()) {
    pbl_panic();
  }

  return (pbl_mutex_lock(&mu->handle, PBL_TICKS(timeout)) == 0) ? BLE_NPL_OK : BLE_NPL_TIMEOUT;
}

ble_npl_error_t npl_pebble_mutex_release(struct ble_npl_mutex *mu) {
  if (!mu) {
    return BLE_NPL_INVALID_PARAM;
  }

  pbl_mutex_unlock(&mu->handle);

  return BLE_NPL_OK;
}

ble_npl_error_t npl_pebble_sem_init(struct ble_npl_sem *sem, uint16_t tokens) {
  if (!sem) {
    return BLE_NPL_INVALID_PARAM;
  }

  pbl_sem_init(&sem->handle, tokens, 128);
  return BLE_NPL_OK;
}

ble_npl_error_t npl_pebble_sem_pend(struct ble_npl_sem *sem, ble_npl_time_t timeout) {
  if (!sem) {
    return BLE_NPL_INVALID_PARAM;
  }

  if (mcu_state_is_isr()) {
    assert(timeout == 0);
  }

  return pbl_sem_take(&sem->handle, PBL_TICKS(timeout)) == 0 ? BLE_NPL_OK : BLE_NPL_TIMEOUT;
}

ble_npl_error_t npl_pebble_sem_release(struct ble_npl_sem *sem) {
  if (!sem) {
    return BLE_NPL_INVALID_PARAM;
  }

  pbl_sem_give(&sem->handle);
  return BLE_NPL_OK;
}

static void os_callout_timer_cb(struct pbl_work *work) {
  struct ble_npl_callout *co = PBL_CONTAINER_OF(work, struct ble_npl_callout, work.work);

  if (co->evq) {
    ble_npl_eventq_put(co->evq, &co->ev);
  } else {
    co->ev.fn(&co->ev);
  }
}

void npl_pebble_callout_init(struct ble_npl_callout *co, struct ble_npl_eventq *evq,
                             ble_npl_event_fn *ev_cb, void *ev_arg) {
  // NimBLE inits some callouts again on every host start, possibly while pending. All of them are
  // in static storage, so this only matches an earlier init.
  if (co->work.work.fn == os_callout_timer_cb) {
    pbl_delayed_work_cancel(&co->work);
  }

  memset(co, 0, sizeof(*co));

  pbl_delayed_work_init(&co->work, os_callout_timer_cb);
  co->evq = evq;

  ble_npl_event_init(&co->ev, ev_cb, ev_arg);
}

ble_npl_error_t npl_pebble_callout_reset(struct ble_npl_callout *co, ble_npl_time_t ticks) {
  co->ticks = ticks;

  // Never run from here, so that a callout always runs from the same context.
  pbl_delayed_work_reschedule(&pbl_sys_workq_high, &co->work, PBL_TICKS(PBL_MAX(ticks, 1U)));

  return BLE_NPL_OK;
}

void npl_pebble_callout_stop(struct ble_npl_callout *co) {
  pbl_delayed_work_cancel(&co->work);
}

bool npl_pebble_callout_is_active(struct ble_npl_callout *co) {
  return pbl_delayed_work_is_pending(&co->work);
}

ble_npl_time_t npl_pebble_callout_get_ticks(struct ble_npl_callout *co) {
  return co->ticks;
}

ble_npl_time_t npl_pebble_callout_remaining_ticks(struct ble_npl_callout *co, ble_npl_time_t now) {
  return pbl_ticks_to_ms(pbl_delayed_work_remaining(&co->work));
}

ble_npl_error_t npl_pebble_time_ms_to_ticks(uint32_t ms, ble_npl_time_t *out_ticks) {
  uint64_t ticks;

  ticks = pbl_ms_to_ticks(ms);
  if (ticks > UINT32_MAX) {
    return BLE_NPL_EINVAL;
  }

  *out_ticks = ticks;

  return 0;
}

ble_npl_error_t npl_pebble_time_ticks_to_ms(ble_npl_time_t ticks, uint32_t *out_ms) {
  uint64_t ms;

  ms = pbl_ticks_to_ms(ticks);
  if (ms > UINT32_MAX) {
    return BLE_NPL_EINVAL;
  }

  *out_ms = ms;

  return 0;
}
