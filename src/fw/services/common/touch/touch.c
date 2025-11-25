/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "touch.h"
#include "touch_event.h"
#include "touch_client.h"

#include "kernel/events.h"
#include "system/logging.h"
#include "os/mutex.h"
#include "system/passert.h"
#include "util/size.h"

#define TOUCH_DEBUG(fmt, args...) PBL_LOG_D(LOG_DOMAIN_TOUCH, LOG_LEVEL_DEBUG, fmt, ## args)

#define TOUCH_QUEUE_LENGTH (2)

typedef struct TouchContext {
  TouchState state;
  bool update_pending;
  bool update_cancelled;
  struct {
    TouchEvent events[TOUCH_QUEUE_LENGTH];
    uint32_t count;
    uint32_t read_idx;
  } touch_queue;
  GPoint start_pos;
  TouchPressure start_pressure;
  uint64_t start_time_ms;
} TouchContext;

static TouchContext s_touch_ctx[MAX_NUM_TOUCHES];

static PebbleMutex *s_touch_mutex;

void touch_init(void) {
  s_touch_mutex = mutex_create();
}

// This should be called within a mutex protected section!
static void prv_touch_queue_reset(TouchContext *ctx) {
  ctx->touch_queue.count = 0;
  ctx->touch_queue.read_idx = 0;
}

// This should be called within a mutex protected section!
static void prv_touch_context_reset(TouchContext *ctx) {
  ctx->state = TouchState_FingerUp;
  prv_touch_queue_reset(ctx);
}

static bool prv_queue_is_full(TouchContext *ctx) {
  return ctx->touch_queue.count == ARRAY_LENGTH(ctx->touch_queue.events);
}

// Note: Do not call if the queue count is zero
static uint32_t prv_get_idx_last_item(TouchContext *ctx) {
  return (ctx->touch_queue.read_idx + ctx->touch_queue.count - 1) %
        ARRAY_LENGTH(ctx->touch_queue.events);
}

static void prv_touch_queue_add(TouchContext *ctx, TouchIdx touch_idx, TouchEventType type,
                                 const GPoint *pos, TouchPressure pressure, uint64_t time_ms) {
  // Note: pos might be NULL for liftoff events
  GPoint diff_pos = pos ? gpoint_sub(*pos, ctx->start_pos) : GPointZero;
  int64_t diff_time = time_ms - ctx->start_time_ms;
  TouchPressure diff_pressure = pressure - ctx->start_pressure;
  uint32_t queue_idx;
  if (!prv_queue_is_full(ctx)) {
    queue_idx = (ctx->touch_queue.read_idx + ctx->touch_queue.count) %
        ARRAY_LENGTH(ctx->touch_queue.events);
    ctx->touch_queue.count++;
  } else {
    // overwrite the last event
    queue_idx = prv_get_idx_last_item(ctx);
    if (!pos) {
      diff_pos = ctx->touch_queue.events[queue_idx].diff_pos;
    }
  }
  TouchEvent *event = &ctx->touch_queue.events[queue_idx];
  *event = (TouchEvent) {
    .index = touch_idx,
    .type = type,
    .start_pos = ctx->start_pos,
    .start_time_ms = ctx->start_time_ms,
    .start_pressure = ctx->start_pressure,
    .diff_pos = (type == TouchEvent_Touchdown) ? GPointZero : diff_pos,
    .diff_time_ms = (type == TouchEvent_Touchdown) ? 0 : diff_time,
    .diff_pressure = (type == TouchEvent_Touchdown) ? 0 : diff_pressure
  };
}

void touch_handle_update(TouchIdx touch_idx, TouchState touch_state, const GPoint *pos,
                         TouchPressure pressure, uint64_t time_ms) {
  PBL_ASSERTN(touch_idx < MAX_NUM_TOUCHES);

  mutex_lock(s_touch_mutex);

  TouchContext *ctx = &s_touch_ctx[touch_idx];
  bool update = false;
  if (ctx->state != touch_state) {
    if (touch_state == TouchState_FingerDown) {
      PBL_ASSERTN(pos);

      // Reset all state when a touchdown event occurs
      prv_touch_context_reset(ctx);
      ctx->start_pos = *pos;
      ctx->start_time_ms = time_ms;
      ctx->start_pressure = pressure;
      prv_touch_queue_add(ctx, touch_idx, TouchEvent_Touchdown, pos, pressure, time_ms);
      TOUCH_DEBUG("Touch %"PRIu8": Touchdown @ (%"PRId16", %"PRId16")", touch_idx,
              pos->x, pos->y);
    } else {
      prv_touch_queue_add(ctx, touch_idx, TouchEvent_Liftoff, pos, 0, time_ms);
      TOUCH_DEBUG("Touch %"PRIu8": Liftoff!", touch_idx);
    }
    update = true;
  } else if (touch_state == TouchState_FingerDown) {
    PBL_ASSERTN(pos);
    if (ctx->touch_queue.count > 0) {
      // don't update if the position hasn't changed
      TouchEvent *last = &ctx->touch_queue.events[prv_get_idx_last_item(ctx)];
      GPoint last_pos = gpoint_add(last->start_pos, last->diff_pos);
      update = !gpoint_equal(&last_pos, pos);
    } else {
      update = true;
    }
    if (update) {
      TOUCH_DEBUG("Touch %"PRIu8": Position Update @ (%"PRId16", %"PRId16")", touch_idx,
              pos->x, pos->y);
      prv_touch_queue_add(ctx, touch_idx, TouchEvent_PositionUpdate, pos, pressure, time_ms);
    }
  }
  ctx->state = touch_state;

  bool send_event = false;
  if (update && !ctx->update_pending) {
    ctx->update_pending = true;
    send_event = true;
  }

  mutex_unlock(s_touch_mutex);

  if (send_event) {
    PebbleEvent e = {
      .type = PEBBLE_TOUCH_EVENT,
      .touch = {
        .type = PebbleTouchEvent_TouchesAvailable,
        .touch_idx = touch_idx
      }
    };
    event_put(&e);
  }
}

void touch_dispatch_touch_events(TouchIdx touch_idx, TouchEventHandler event_handler,
                                 void *context) {
  PBL_ASSERTN(event_handler);
  PBL_ASSERTN(touch_idx < MAX_NUM_TOUCHES);

  TouchContext *ctx = &s_touch_ctx[touch_idx];
  mutex_lock(s_touch_mutex);

  if (ctx->update_cancelled) {
    ctx->update_cancelled = false;
    goto unlock;
  }
  if (!ctx->update_pending) {
    goto unlock;
  }
  while (ctx->touch_queue.count > 0) {
    // Copy touch event so that resetting the touch queue does not trash the event data
    TouchEvent event = ctx->touch_queue.events[ctx->touch_queue.read_idx];

    // Update the queue position before unlocking so if the state changes during the delivery
    // callback, we don't overwrite the change
    ctx->touch_queue.read_idx = (ctx->touch_queue.read_idx + 1) %
        ARRAY_LENGTH(ctx->touch_queue.events);
    ctx->touch_queue.count--;

    // unlock the mutex so that any calculations done in the callback do not block touch updates
    // from the driver.
    mutex_unlock(s_touch_mutex);
    event_handler(&event, context);
    mutex_lock(s_touch_mutex);
  }
  ctx->update_pending = false;

unlock:
  mutex_unlock(s_touch_mutex);
}

void touch_handle_driver_event(TouchDriverEvent driver_event) {
  PBL_ASSERTN(driver_event < TouchDriverEventCount);
  mutex_lock(s_touch_mutex);

  for (uint32_t i = 0; i < ARRAY_LENGTH(s_touch_ctx); i++) {
    TouchContext *ctx = &s_touch_ctx[i];
    prv_touch_context_reset(ctx);

    // If there is an event on the kernel queue, we need to set a flag to not dispatch touches in
    // the queue when that event is called, because the TouchesCancelled event will arrive after
    // the event on the queue. We do however want to be able to handle any touch events that happen
    // afterwards, so we might have a touches available event (which would receive no touches),
    // followed by a touches cancelled event, followed by another touches available event (which
    // would receive touches).
    if (ctx->update_pending) {
      ctx->update_cancelled = true;
    }
    ctx->update_pending = false;
  }

  mutex_unlock(s_touch_mutex);

  // Always send a touches cancelled event (all currently defined events cancel other touches)
  PebbleEvent cancel_event = {
    .type = PEBBLE_TOUCH_EVENT,
    .touch = {
      .type = PebbleTouchEvent_TouchesCancelled,
    }
  };
  event_put(&cancel_event);

  if (driver_event == TouchDriverEvent_PalmDetect) {
    PebbleEvent palm_event = {
      .type = PEBBLE_TOUCH_EVENT,
      .touch = {
        .type = PebbleTouchEvent_PalmDetected,
      }
    };
    event_put(&palm_event);
  }
}

void touch_reset(void) {
  mutex_lock(s_touch_mutex);

  for (uint32_t i = 0; i < ARRAY_LENGTH(s_touch_ctx); i++) {
    TouchContext *ctx = &s_touch_ctx[i];
    prv_touch_context_reset(ctx);
    ctx->update_pending = false;
    ctx->update_cancelled = false;
  }

  mutex_unlock(s_touch_mutex);
}

#if UNITTEST
TouchEvent *touch_event_queue_get_event(TouchIdx touch_idx, uint32_t queue_idx) {
  if ((touch_idx > ARRAY_LENGTH(s_touch_ctx)) ||
      (queue_idx > ARRAY_LENGTH(s_touch_ctx[touch_idx].touch_queue.events)) ||
      (queue_idx >= s_touch_ctx[touch_idx].touch_queue.count)) {
    return NULL;
  }
  queue_idx = (s_touch_ctx[touch_idx].touch_queue.read_idx + queue_idx) %
      ARRAY_LENGTH(s_touch_ctx[touch_idx].touch_queue.events);
  return &s_touch_ctx[touch_idx].touch_queue.events[queue_idx];
}

void touch_set_touch_state(TouchIdx touch_idx, TouchState touch_state, GPoint touch_down_pos,
                           uint64_t touch_down_time_ms, TouchPressure touch_down_pressure) {
  if (touch_idx > ARRAY_LENGTH(s_touch_ctx)) {
    return;
  }
  s_touch_ctx[touch_idx].start_pos = touch_down_pos;
  s_touch_ctx[touch_idx].start_time_ms = touch_down_time_ms;
  s_touch_ctx[touch_idx].start_pressure = touch_down_pressure;
  s_touch_ctx[touch_idx].state = touch_state;
}
#endif
