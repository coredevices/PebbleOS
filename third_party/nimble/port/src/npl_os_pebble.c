/*
 * Copyright 2025 Google LLC
 * Copyright 2015-2024 The Apache Software Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This is derived from the freertos port provided by NimBLE
// and modified to suit Pebble OS (timers, mutexes).

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "mcu/interrupts.h"
#include "nimble/nimble_npl.h"
#include "os/mutex.h"
#include "os/tick.h"
#include "services/common/new_timer/new_timer.h"
#include "system/logging.h"
#include "system/passert.h"

struct ble_npl_event *npl_pebble_eventq_get(struct ble_npl_eventq *evq, ble_npl_time_t tmo) {
  struct ble_npl_event *ev = NULL;
  BaseType_t woken;
  BaseType_t ret;

  if (mcu_state_is_isr()) {
    assert(tmo == 0);
    ret = xQueueReceiveFromISR(evq->q, &ev, &woken);
    portYIELD_FROM_ISR(woken);
  } else {
    ret = xQueueReceive(evq->q, &ev, tmo);
  }
  assert(ret == pdPASS || ret == errQUEUE_EMPTY);

  if (ev) {
    ev->queued = false;
  }

  return ev;
}

void npl_pebble_eventq_put(struct ble_npl_eventq *evq, struct ble_npl_event *ev) {
  BaseType_t woken;
  BaseType_t ret;

  if (ev->queued) {
    return;
  }

  ev->queued = true;

  if (mcu_state_is_isr()) {
    ret = xQueueSendToBackFromISR(evq->q, &ev, &woken);
    portYIELD_FROM_ISR(woken);
  } else {
    ret = xQueueSendToBack(evq->q, &ev, portMAX_DELAY);
  }

  assert(ret == pdPASS);
}

void npl_pebble_eventq_remove(struct ble_npl_eventq *evq, struct ble_npl_event *ev) {
  struct ble_npl_event *tmp_ev;
  BaseType_t ret;
  int i;
  int count;
  BaseType_t woken, woken2;

  if (!ev->queued) {
    return;
  }

  /*
   * XXX We cannot extract element from inside FreeRTOS queue so as a quick
   * workaround we'll just remove all elements and add them back except the
   * one we need to remove. This is silly, but works for now - we probably
   * better use counting semaphore with os_queue to handle this in future.
   */

  if (mcu_state_is_isr()) {
    woken = pdFALSE;

    count = uxQueueMessagesWaitingFromISR(evq->q);
    for (i = 0; i < count; i++) {
      ret = xQueueReceiveFromISR(evq->q, &tmp_ev, &woken2);
      assert(ret == pdPASS);
      woken |= woken2;

      if (tmp_ev == ev) {
        continue;
      }

      ret = xQueueSendToBackFromISR(evq->q, &tmp_ev, &woken2);
      assert(ret == pdPASS);
      woken |= woken2;
    }

    portYIELD_FROM_ISR(woken);
  } else {
    vPortEnterCritical();

    count = uxQueueMessagesWaiting(evq->q);
    for (i = 0; i < count; i++) {
      ret = xQueueReceive(evq->q, &tmp_ev, 0);
      assert(ret == pdPASS);

      if (tmp_ev == ev) {
        continue;
      }

      ret = xQueueSendToBack(evq->q, &tmp_ev, 0);
      assert(ret == pdPASS);
    }

    vPortExitCritical();
  }

  ev->queued = 0;
}

ble_npl_error_t npl_pebble_mutex_init(struct ble_npl_mutex *mu) {
  if (!mu) {
    return BLE_NPL_INVALID_PARAM;
  }

  mu->handle = mutex_create_recursive();
  assert(mu->handle);

  return BLE_NPL_OK;
}

ble_npl_error_t npl_pebble_mutex_pend(struct ble_npl_mutex *mu, ble_npl_time_t timeout) {
  if (!mu) {
    return BLE_NPL_INVALID_PARAM;
  }

  assert(mu->handle);

  if (mcu_state_is_isr()) {
    WTF;
  }

  uint32_t ms;
  ble_npl_time_ticks_to_ms(timeout, &ms);
  return mutex_lock_recursive_with_timeout(mu->handle, ms) ? BLE_NPL_OK : BLE_NPL_TIMEOUT;
}

ble_npl_error_t npl_pebble_mutex_release(struct ble_npl_mutex *mu) {
  if (!mu) {
    return BLE_NPL_INVALID_PARAM;
  }

  assert(mu->handle);

  mutex_unlock_recursive(mu->handle);

  return BLE_NPL_OK;
}

ble_npl_error_t npl_pebble_sem_init(struct ble_npl_sem *sem, uint16_t tokens) {
  if (!sem) {
    return BLE_NPL_INVALID_PARAM;
  }

  sem->handle = xSemaphoreCreateCounting(128, tokens);
  assert(sem->handle);

  return BLE_NPL_OK;
}

ble_npl_error_t npl_pebble_sem_pend(struct ble_npl_sem *sem, ble_npl_time_t timeout) {
  BaseType_t woken;
  BaseType_t ret;

  if (!sem) {
    return BLE_NPL_INVALID_PARAM;
  }

  assert(sem->handle);

  if (mcu_state_is_isr()) {
    assert(timeout == 0);
    ret = xSemaphoreTakeFromISR(sem->handle, &woken);
    portYIELD_FROM_ISR(woken);
  } else {
    ret = xSemaphoreTake(sem->handle, timeout);
  }

  return ret == pdPASS ? BLE_NPL_OK : BLE_NPL_TIMEOUT;
}

ble_npl_error_t npl_pebble_sem_release(struct ble_npl_sem *sem) {
  BaseType_t ret;
  BaseType_t woken;

  if (!sem) {
    return BLE_NPL_INVALID_PARAM;
  }

  assert(sem->handle);

  if (mcu_state_is_isr()) {
    ret = xSemaphoreGiveFromISR(sem->handle, &woken);
    portYIELD_FROM_ISR(woken);
  } else {
    ret = xSemaphoreGive(sem->handle);
  }

  assert(ret == pdPASS);
  return BLE_NPL_OK;
}

static void os_callout_timer_cb(void *timer) {
  struct ble_npl_callout *co = timer;

  if (co->evq) {
    ble_npl_eventq_put(co->evq, &co->ev);
  } else {
    co->ev.fn(&co->ev);
  }
}

void npl_pebble_callout_init(struct ble_npl_callout *co, struct ble_npl_eventq *evq,
                             ble_npl_event_fn *ev_cb, void *ev_arg) {
  memset(co, 0, sizeof(*co));

  co->handle = new_timer_create();
  PBL_ASSERTN(co->handle != TIMER_INVALID_ID);
  co->evq = evq;
  co->ticks = 0;

  ble_npl_event_init(&co->ev, ev_cb, ev_arg);
}

ble_npl_error_t npl_pebble_callout_reset(struct ble_npl_callout *co, ble_npl_time_t ticks) {
  new_timer_stop(co->handle);
  uint32_t ms;
  ble_npl_time_ticks_to_ms(ticks, &ms);
  PBL_ASSERTN(new_timer_start(co->handle, ms, os_callout_timer_cb, co, 0));
  co->ticks = ticks;
  return BLE_NPL_OK;
}

void npl_pebble_callout_stop(struct ble_npl_callout *co) { new_timer_stop(co->handle); }

bool npl_pebble_callout_is_active(struct ble_npl_callout *co) {
  return new_timer_scheduled(co->handle, NULL);
}

ble_npl_time_t npl_pebble_callout_get_ticks(struct ble_npl_callout *co) { return co->ticks; }

uint32_t npl_pebble_callout_remaining_ticks(struct ble_npl_callout *co, ble_npl_time_t now) {
  uint32_t rt = 0;
  new_timer_scheduled(co->handle, &rt);
  return rt;
}

ble_npl_error_t npl_pebble_time_ms_to_ticks(uint32_t ms, ble_npl_time_t *out_ticks) {
  uint64_t ticks;

  ticks = milliseconds_to_ticks(ms);
  if (ticks > UINT32_MAX) {
    return BLE_NPL_EINVAL;
  }

  *out_ticks = ticks;

  return 0;
}

ble_npl_error_t npl_pebble_time_ticks_to_ms(ble_npl_time_t ticks, uint32_t *out_ms) {
  uint64_t ms;

  ms = ticks_to_milliseconds(ticks);
  if (ms > UINT32_MAX) {
    return BLE_NPL_EINVAL;
  }

  *out_ms = ms;

  return 0;
}

void __assert_func(const char *file, int line, const char *func, const char *e) {
  PBL_LOG_D(LOG_DOMAIN_BT, LOG_LEVEL_ERROR, "Nimble assert at line %d, func: %s - %s", line, func, e);
  WTF;
}
