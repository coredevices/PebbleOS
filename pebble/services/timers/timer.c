/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/cron/cron.h>
#include <pbl/kernel/mutex.h>
#include <pbl/kernel/time.h>
#include <pbl/logging/logging.h>
#include <pbl/services/timers/timer.h>
#include <pbl/util/misc.h>
#include <pbl/util/uuid.h>

#include <kernel/events.h>

PBL_LOG_MODULE_DEFINE(service_timers, CONFIG_SERVICE_TIMERS_LOG_LEVEL);

typedef enum TimerState {
  TimerState_Free = 0,
  TimerState_Creating,
  TimerState_Running,
  TimerState_Cancelled,
} TimerState;

typedef struct Timer {
  TimerId id;
  TimerState state;
  Uuid owner;
  uint32_t duration_s;
  time_t deadline;
  struct pbl_cron_job job;
} Timer;

static Timer s_timers[TIMER_MAX_COUNT];
static TimerId s_next_id = 1;

static PBL_MUTEX_DEFINE(s_mutex);

static void prv_release(Timer *timer) {
  timer->id = TIMER_INVALID_ID;
  timer->state = TimerState_Free;
}

static Timer *prv_find_locked(TimerId id) {
  for (int i = 0; i < TIMER_MAX_COUNT; ++i) {
    Timer *timer = &s_timers[i];
    if (timer->state != TimerState_Free && timer->id == id) {
      return timer;
    }
  }
  return NULL;
}

static TimerId prv_next_id(void) {
  const TimerId id = s_next_id++;
  if (s_next_id <= 0) {
    s_next_id = 1;
  }
  return id;
}

// Runs on the system work queue. The slot may already have been reused by the
// time this returns, so copy everything needed before releasing it.
static void prv_timer_cb(struct pbl_cron_job *job, void *data) {
  Timer *timer = PBL_CONTAINER_OF(job, Timer, job);

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  if (timer->state == TimerState_Free) {
    pbl_mutex_unlock(&s_mutex);
    return;
  }
  const bool cancelled = timer->state == TimerState_Cancelled;
  const uint32_t duration_s = timer->duration_s;
  prv_release(timer);
  pbl_mutex_unlock(&s_mutex);

  if (cancelled) {
    return;
  }

  PBL_LOG_DBG("Timer fired after %u s", (unsigned int)duration_s);
  PebbleEvent event = {
    .type = PEBBLE_TIMER_EVENT,
    .timer = {.duration_s = duration_s},
  };
  event_put(&event);
}

void timer_init(void) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  for (int i = 0; i < TIMER_MAX_COUNT; ++i) {
    prv_release(&s_timers[i]);
  }
  s_next_id = 1;
  pbl_mutex_unlock(&s_mutex);
}

bool timer_can_create(void) {
  bool free_slot = false;
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  for (int i = 0; i < TIMER_MAX_COUNT; ++i) {
    if (s_timers[i].state == TimerState_Free) {
      free_slot = true;
      break;
    }
  }
  pbl_mutex_unlock(&s_mutex);
  return free_slot;
}

TimerId timer_start(uint32_t duration_s, const Uuid *owner) {
  if (!owner || duration_s == 0 || duration_s > TIMER_MAX_DURATION_S) {
    return TIMER_INVALID_ID;
  }

  Timer *timer = NULL;
  TimerId id = TIMER_INVALID_ID;

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  for (int i = 0; i < TIMER_MAX_COUNT; ++i) {
    if (s_timers[i].state == TimerState_Free) {
      timer = &s_timers[i];
      break;
    }
  }
  if (timer) {
    id = prv_next_id();
    timer->id = id;
    timer->state = TimerState_Creating;
    timer->owner = *owner;
    timer->duration_s = duration_s;
    timer->deadline = pbl_realtime() + duration_s;
    timer->job = (struct pbl_cron_job){
      .cb = prv_timer_cb,
      .cb_data = NULL,
    };
  }
  pbl_mutex_unlock(&s_mutex);

  if (!timer) {
    return TIMER_INVALID_ID;
  }

  // Not under the lock: the cron callback can run as soon as the job is
  // scheduled and it takes the same lock.
  pbl_cron_job_schedule_at(&timer->job, timer->deadline);

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  if (timer->id == id) {
    if (timer->state == TimerState_Creating) {
      timer->state = TimerState_Running;
    } else if (timer->state == TimerState_Cancelled) {
      // A cancel slipped in before the job was scheduled. If the job is
      // already running, the callback releases the slot itself.
      if (pbl_cron_job_unschedule(&timer->job)) {
        prv_release(timer);
      }
    }
  }
  pbl_mutex_unlock(&s_mutex);

  PBL_LOG_DBG("Timer %d created for %u s", (int)id, (unsigned int)duration_s);
  return id;
}

bool timer_cancel(TimerId id) {
  bool cancelled = false;
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  Timer *timer = prv_find_locked(id);
  if (timer) {
    timer->state = TimerState_Cancelled;
    // False when the callback is already running or the job is not scheduled
    // yet; both paths release the slot themselves.
    if (pbl_cron_job_unschedule(&timer->job)) {
      prv_release(timer);
    }
    cancelled = true;
  }
  pbl_mutex_unlock(&s_mutex);
  return cancelled;
}

uint32_t timer_cancel_all(const Uuid *owner) {
  if (!owner) {
    return 0;
  }

  uint32_t count = 0;
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  for (int i = 0; i < TIMER_MAX_COUNT; ++i) {
    Timer *timer = &s_timers[i];
    if (timer->state == TimerState_Free || !uuid_equal(&timer->owner, owner)) {
      continue;
    }
    timer->state = TimerState_Cancelled;
    if (pbl_cron_job_unschedule(&timer->job)) {
      prv_release(timer);
    }
    count++;
  }
  pbl_mutex_unlock(&s_mutex);
  return count;
}

uint8_t timer_get_timers(TimerInfo *out, uint8_t max) {
  if (!out || max == 0) {
    return 0;
  }

  TimerInfo entries[TIMER_MAX_COUNT];
  uint8_t count = 0;
  const time_t now = pbl_realtime();

  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  for (int i = 0; i < TIMER_MAX_COUNT; ++i) {
    Timer *timer = &s_timers[i];
    if (timer->state != TimerState_Running) {
      continue;
    }
    TimerInfo *entry = &entries[count++];
    entry->id = timer->id;
    entry->duration_s = timer->duration_s;
    const time_t remaining = timer->deadline - now;
    entry->remaining_s = remaining > 0 ? (uint32_t)remaining : 0;
    timer_format_duration(entry->description, sizeof(entry->description), timer->duration_s);
  }
  pbl_mutex_unlock(&s_mutex);

  // Soonest first.
  for (int i = 1; i < count; ++i) {
    const TimerInfo key = entries[i];
    int j = i - 1;
    while (j >= 0 && entries[j].remaining_s > key.remaining_s) {
      entries[j + 1] = entries[j];
      --j;
    }
    entries[j + 1] = key;
  }

  const uint8_t written = count < max ? count : max;
  memcpy(out, entries, written * sizeof(*out));
  return written;
}
