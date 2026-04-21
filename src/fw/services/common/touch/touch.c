/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pbl/services/common/touch/touch.h"
#include "pbl/services/common/touch/touch_event.h"

#include "drivers/touch/touch_sensor.h"
#include "kernel/events.h"
#include "kernel/pebble_tasks.h"
#include "pbl/services/common/event_service.h"
#include "pbl/services/common/analytics/analytics.h"
#include "system/logging.h"
#include "os/mutex.h"
#include "system/passert.h"

#define TOUCH_DEBUG(fmt, args...) PBL_LOG_D_DBG(LOG_DOMAIN_TOUCH, fmt, ##args)

static TouchState s_touch_state = TouchState_FingerUp;
static int16_t s_last_x;
static int16_t s_last_y;

static PebbleMutex *s_touch_mutex;

static uint8_t s_subscriber_count = 0;
static bool s_backlight_subscribed = false;

static void prv_add_subscriber_cb(PebbleTask task) {
  mutex_lock(s_touch_mutex);
  if (++s_subscriber_count == 1) {
    touch_sensor_set_enabled(true);
  }
  mutex_unlock(s_touch_mutex);
}

static void prv_remove_subscriber_cb(PebbleTask task) {
  mutex_lock(s_touch_mutex);
  PBL_ASSERTN(s_subscriber_count > 0);
  if (--s_subscriber_count == 0) {
    touch_sensor_set_enabled(false);
  }
  mutex_unlock(s_touch_mutex);
}

void touch_init(void) {
  s_touch_mutex = mutex_create();

  event_service_init(PEBBLE_TOUCH_EVENT, &prv_add_subscriber_cb,
      &prv_remove_subscriber_cb);
}

bool touch_has_app_subscribers(void) {
  mutex_lock(s_touch_mutex);
  const bool has_apps = s_subscriber_count > 0;
  mutex_unlock(s_touch_mutex);
  return has_apps;
}

void touch_set_backlight_enabled(bool enabled) {
  mutex_lock(s_touch_mutex);
  if (enabled && !s_backlight_subscribed) {
    s_backlight_subscribed = true;
    mutex_unlock(s_touch_mutex);
    prv_add_subscriber_cb(PebbleTask_KernelMain);
    return;
  } else if (!enabled && s_backlight_subscribed) {
    s_backlight_subscribed = false;
    mutex_unlock(s_touch_mutex);
    prv_remove_subscriber_cb(PebbleTask_KernelMain);
    return;
  }
  mutex_unlock(s_touch_mutex);
}

static void prv_put_touch_event(TouchEventType type, int16_t x, int16_t y) {
  PebbleEvent e = {
    .type = PEBBLE_TOUCH_EVENT,
    .touch = {
      .event = {
        .type = type,
        .x = x,
        .y = y,
      },
    },
  };
  event_put(&e);
}

void touch_handle_update(TouchState touch_state, int16_t x, int16_t y) {
  mutex_lock(s_touch_mutex);

  if (s_touch_state != touch_state) {
    s_touch_state = touch_state;
    s_last_x = x;
    s_last_y = y;
    mutex_unlock(s_touch_mutex);

    if (touch_state == TouchState_FingerDown) {
      PBL_ANALYTICS_ADD(touch_event_count, 1);
      TOUCH_DEBUG("Touch: Touchdown @ (%" PRId16 ", %" PRId16 ")", x, y);
      prv_put_touch_event(TouchEvent_Touchdown, x, y);
    } else {
      TOUCH_DEBUG("Touch: Liftoff");
      prv_put_touch_event(TouchEvent_Liftoff, x, y);
    }
    return;
  }

  if (touch_state == TouchState_FingerDown && (x != s_last_x || y != s_last_y)) {
    s_last_x = x;
    s_last_y = y;
    mutex_unlock(s_touch_mutex);

    TOUCH_DEBUG("Touch: Position Update @ (%" PRId16 ", %" PRId16 ")", x, y);
    prv_put_touch_event(TouchEvent_PositionUpdate, x, y);
    return;
  }

  mutex_unlock(s_touch_mutex);
}

void touch_reset(void) {
  mutex_lock(s_touch_mutex);
  s_touch_state = TouchState_FingerUp;
  s_last_x = 0;
  s_last_y = 0;
  mutex_unlock(s_touch_mutex);
}
