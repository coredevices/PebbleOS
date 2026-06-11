/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "drivers/rtc.h"
#include "hal/hal_timer.h"
#include "pbl/services/new_timer/new_timer.h"
#include "system/passert.h"

#define QEMU_HAL_TIMER_MAX (6)
#define QEMU_HAL_TIMER_DEFAULT_FREQ_HZ (32768)

typedef struct QemuHalTimer {
  bool initialized;
  uint32_t freq_hz;
  TimerID handle;
  uint32_t generation;
  TAILQ_HEAD(QemuHalTimerQueue, hal_timer) queue;
} QemuHalTimer;

static QemuHalTimer s_qemu_hal_timers[QEMU_HAL_TIMER_MAX];

static void prv_timer_cb(void *data);

static QemuHalTimer *prv_timer_for_num(int timer_num) {
  if (timer_num < 0 || timer_num >= QEMU_HAL_TIMER_MAX) {
    return NULL;
  }

  return &s_qemu_hal_timers[timer_num];
}

static uint32_t prv_read_timer(const QemuHalTimer *timer) {
  return (uint32_t)((rtc_get_ticks() * timer->freq_hz) / RTC_TICKS_HZ);
}

uint32_t hal_timer_read(int timer_num) {
  QemuHalTimer *timer = prv_timer_for_num(timer_num);
  if (!timer || !timer->initialized) {
    return 0;
  }

  return prv_read_timer(timer);
}

static uint32_t prv_ticks_to_ms_ceil(QemuHalTimer *timer, uint32_t ticks) {
  uint64_t ms = ((uint64_t)ticks * 1000U + timer->freq_hz - 1U) / timer->freq_hz;
  if (ms == 0) {
    ms = 1;
  }
  if (ms > UINT32_MAX) {
    ms = UINT32_MAX;
  }
  return (uint32_t)ms;
}

static bool prv_prepare_next_locked(QemuHalTimer *timer, uint32_t *delay_ms_out) {
  struct hal_timer *next = TAILQ_FIRST(&timer->queue);
  if (!next) {
    return false;
  }

  const uint32_t now = prv_read_timer(timer);
  const int32_t delta = (int32_t)(next->expiry - now);
  *delay_ms_out = (delta <= 0) ? 1 : prv_ticks_to_ms_ceil(timer, (uint32_t)delta);
  return true;
}

static void prv_schedule_next(QemuHalTimer *timer) {
  while (true) {
    uint32_t generation;
    uint32_t delay_ms = 0;

    vPortEnterCritical();
    generation = timer->generation;
    const bool has_next = prv_prepare_next_locked(timer, &delay_ms);
    vPortExitCritical();

    if (has_next) {
      PBL_ASSERTN(new_timer_start(timer->handle, delay_ms, prv_timer_cb, timer, 0));
    } else {
      new_timer_stop(timer->handle);
    }

    vPortEnterCritical();
    const bool stable = (generation == timer->generation);
    vPortExitCritical();
    if (stable) {
      return;
    }
  }
}

static void prv_timer_cb(void *data) {
  QemuHalTimer *timer = data;

  while (true) {
    hal_timer_cb cb = NULL;
    void *cb_arg = NULL;

    vPortEnterCritical();
    struct hal_timer *next = TAILQ_FIRST(&timer->queue);
    const uint32_t now = prv_read_timer(timer);
    if (!next || (int32_t)(now - next->expiry) < 0) {
      vPortExitCritical();
      prv_schedule_next(timer);
      return;
    }

    TAILQ_REMOVE(&timer->queue, next, link);
    next->link.tqe_prev = NULL;
    ++timer->generation;
    cb = next->cb_func;
    cb_arg = next->cb_arg;
    vPortExitCritical();

    cb(cb_arg);
  }
}

int hal_timer_init(int timer_num, void *cfg) {
  QemuHalTimer *timer = prv_timer_for_num(timer_num);
  if (!timer) {
    return EINVAL;
  }

  if (timer->initialized) {
    return 0;
  }

  memset(timer, 0, sizeof(*timer));
  timer->freq_hz = QEMU_HAL_TIMER_DEFAULT_FREQ_HZ;
  timer->handle = new_timer_create();
  PBL_ASSERTN(timer->handle != TIMER_INVALID_ID);
  TAILQ_INIT(&timer->queue);
  timer->initialized = true;
  return 0;
}

int hal_timer_config(int timer_num, uint32_t freq_hz) {
  QemuHalTimer *timer = prv_timer_for_num(timer_num);
  if (!timer || freq_hz == 0) {
    return EINVAL;
  }

  if (!timer->initialized) {
    const int rc = hal_timer_init(timer_num, NULL);
    if (rc != 0) {
      return rc;
    }
  }

  vPortEnterCritical();
  timer->freq_hz = freq_hz;
  ++timer->generation;
  const bool schedule_next = (TAILQ_FIRST(&timer->queue) != NULL);
  vPortExitCritical();

  if (schedule_next) {
    prv_schedule_next(timer);
  }
  return 0;
}

int hal_timer_deinit(int timer_num) {
  QemuHalTimer *timer = prv_timer_for_num(timer_num);
  if (!timer || !timer->initialized) {
    return EINVAL;
  }

  new_timer_stop(timer->handle);
  TAILQ_INIT(&timer->queue);
  timer->initialized = false;
  return 0;
}

uint32_t hal_timer_get_resolution(int timer_num) {
  QemuHalTimer *timer = prv_timer_for_num(timer_num);
  if (!timer || !timer->initialized || timer->freq_hz == 0) {
    return 0;
  }

  return 1000000000U / timer->freq_hz;
}

int hal_timer_delay(int timer_num, uint32_t ticks) {
  const uint32_t until = hal_timer_read(timer_num) + ticks;
  while ((int32_t)(hal_timer_read(timer_num) - until) <= 0) {
  }
  return 0;
}

int hal_timer_set_cb(int timer_num, struct hal_timer *timer, hal_timer_cb cb_func, void *arg) {
  QemuHalTimer *qemu_timer = prv_timer_for_num(timer_num);
  if (!qemu_timer || !qemu_timer->initialized || !timer || !cb_func) {
    return EINVAL;
  }

  timer->bsp_timer = qemu_timer;
  timer->cb_func = cb_func;
  timer->cb_arg = arg;
  timer->link.tqe_prev = NULL;
  return 0;
}

int hal_timer_start(struct hal_timer *timer, uint32_t ticks) {
  if (!timer || !timer->bsp_timer) {
    return EINVAL;
  }

  QemuHalTimer *qemu_timer = timer->bsp_timer;
  return hal_timer_start_at(timer, prv_read_timer(qemu_timer) + ticks);
}

int hal_timer_start_at(struct hal_timer *timer, uint32_t tick) {
  if (!timer || !timer->bsp_timer) {
    return EINVAL;
  }

  QemuHalTimer *qemu_timer = timer->bsp_timer;
  struct hal_timer *entry = NULL;

  vPortEnterCritical();
  if (timer->link.tqe_prev != NULL) {
    TAILQ_REMOVE(&qemu_timer->queue, timer, link);
    timer->link.tqe_prev = NULL;
  }

  timer->expiry = tick;
  TAILQ_FOREACH(entry, &qemu_timer->queue, link) {
    if ((int32_t)(timer->expiry - entry->expiry) < 0) {
      TAILQ_INSERT_BEFORE(entry, timer, link);
      break;
    }
  }
  if (!entry) {
    TAILQ_INSERT_TAIL(&qemu_timer->queue, timer, link);
  }

  ++qemu_timer->generation;
  const bool schedule_next = (timer == TAILQ_FIRST(&qemu_timer->queue));
  vPortExitCritical();

  if (schedule_next) {
    prv_schedule_next(qemu_timer);
  }
  return 0;
}

int hal_timer_stop(struct hal_timer *timer) {
  if (!timer || !timer->bsp_timer) {
    return EINVAL;
  }

  QemuHalTimer *qemu_timer = timer->bsp_timer;

  vPortEnterCritical();
  const bool was_first = (timer == TAILQ_FIRST(&qemu_timer->queue));
  if (timer->link.tqe_prev != NULL) {
    TAILQ_REMOVE(&qemu_timer->queue, timer, link);
    timer->link.tqe_prev = NULL;
    ++qemu_timer->generation;
  }
  const bool schedule_next = was_first;
  vPortExitCritical();

  if (schedule_next) {
    prv_schedule_next(qemu_timer);
  }
  return 0;
}
