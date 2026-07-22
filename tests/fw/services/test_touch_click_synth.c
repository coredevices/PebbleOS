/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Unit tests for the Phase 1 touch->click synthesis bridge (tap -> SELECT).
// Drives the real touch service (touch.c) and bridge (touch_click_synth.c),
// stubbing the surrounding kernel/driver hooks, and asserts:
//   - foreground gating (active only for a focused watchapp),
//   - a tap synthesizes exactly one SELECT down+up,
//   - drags / slow taps do not synthesize,
//   - synthesis is suppressed while an app consumes touch directly.

#include "clar.h"

#include "kernel/events.h"
#include "kernel/pebble_tasks.h"
#include "pbl/services/event_service.h"
#include "pbl/services/touch/touch.h"
#include "pbl/services/touch/touch_event.h"
#include "applib/event_service_client.h"
#include "services/touch/touch_click_synth.h"
#include "drivers/rtc.h"
#include "drivers/button_id.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "fake_events.h"
#include "stubs_analytics.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"

void kernel_free(void *p) {}

// --- touch.c subscriber callbacks (captured via event_service_init) ---
static EventServiceAddSubscriberCallback s_add_subscriber_cb;
static EventServiceRemoveSubscriberCallback s_remove_subscriber_cb;

void event_service_init(PebbleEventType type, EventServiceAddSubscriberCallback add_cb,
                        EventServiceRemoveSubscriberCallback remove_cb) {
  cl_assert(type == PEBBLE_TOUCH_EVENT || type == PEBBLE_GESTURE_EVENT);
  s_add_subscriber_cb = add_cb;
  s_remove_subscriber_cb = remove_cb;
}

static bool s_sensor_enabled;
void touch_sensor_set_enabled(bool enabled) { s_sensor_enabled = enabled; }

// --- the bridge's focus subscription (captured) ---
static EventServiceEventHandler s_focus_handler;
void event_service_client_subscribe(EventServiceInfo *info) {
  if (info->type == PEBBLE_APP_DID_CHANGE_FOCUS_EVENT) {
    s_focus_handler = info->handler;
  }
}
void event_service_client_unsubscribe(EventServiceInfo *info) {}

// --- controllable environment ---
static bool s_is_watchface;
bool app_manager_is_watchface_running(void) { return s_is_watchface; }

static RtcTicks s_ticks;
RtcTicks rtc_get_ticks(void) { return s_ticks; }

// --- capture every posted event (the synthesized SELECT down/up) ---
#define MAX_CAP 8
static PebbleEvent s_cap[MAX_CAP];
static int s_cap_n;
static void prv_capture(PebbleEvent *e) {
  if (s_cap_n < MAX_CAP) {
    s_cap[s_cap_n++] = *e;
  }
}

// --- helpers ---
static int s_net_app_subs;
static void prv_app_subscribe(void) {
  s_add_subscriber_cb(PebbleTask_App);
  s_net_app_subs++;
}
static void prv_app_unsubscribe(void) {
  s_remove_subscriber_cb(PebbleTask_App);
  s_net_app_subs--;
}

static void prv_focus(bool in_focus) {
  PebbleEvent e = {
    .type = PEBBLE_APP_DID_CHANGE_FOCUS_EVENT,
    .app_focus = { .in_focus = in_focus },
  };
  cl_assert(s_focus_handler != NULL);
  s_focus_handler(&e, NULL);
}

static void prv_touch(TouchEventType type, int16_t x, int16_t y) {
  TouchEvent te = { .type = type, .x = x, .y = y };
  touch_click_synth_handle_touch(&te);
}

// A tap: touchdown, small (in-threshold) move, liftoff after `duration_ms`.
static void prv_tap(int16_t x, int16_t y, uint32_t duration_ms) {
  prv_touch(TouchEvent_Touchdown, x, y);
  s_ticks += duration_ms;  // RTC_TICKS_HZ == 1000
  prv_touch(TouchEvent_Liftoff, x + 1, y + 1);
}

// A swipe: touchdown at (sx,sy), a position update and liftoff at (ex,ey).
static void prv_swipe(int16_t sx, int16_t sy, int16_t ex, int16_t ey) {
  prv_touch(TouchEvent_Touchdown, sx, sy);
  prv_touch(TouchEvent_PositionUpdate, ex, ey);
  s_ticks += 50;
  prv_touch(TouchEvent_Liftoff, ex, ey);
}

static void prv_assert_single_click(ButtonId button_id) {
  cl_assert_equal_i(s_cap_n, 2);
  cl_assert_equal_i(s_cap[0].type, PEBBLE_BUTTON_DOWN_EVENT);
  cl_assert_equal_i(s_cap[0].button.button_id, button_id);
  cl_assert_equal_i(s_cap[1].type, PEBBLE_BUTTON_UP_EVENT);
  cl_assert_equal_i(s_cap[1].button.button_id, button_id);
}

void test_touch_click_synth__initialize(void) {
  fake_event_init();
  s_add_subscriber_cb = NULL;
  s_remove_subscriber_cb = NULL;
  s_focus_handler = NULL;
  s_sensor_enabled = false;
  s_is_watchface = false;
  s_ticks = 1000;
  s_cap_n = 0;
  s_net_app_subs = 0;
  memset(s_cap, 0, sizeof(s_cap));

  touch_init();
  touch_reset();
  touch_service_set_globally_enabled(true);
  touch_click_synth_init();

  fake_event_set_callback(prv_capture);
}

void test_touch_click_synth__cleanup(void) {
  fake_event_set_callback(NULL);
  // Release the bridge's sensor hold and any app subscription so touch.c's
  // module-static subscriber count doesn't leak into the next test.
  prv_focus(false);
  while (s_net_app_subs > 0) {
    prv_app_unsubscribe();
  }
}

// --- gating -----------------------------------------------------------------

void test_touch_click_synth__watchapp_focus_activates_sensor(void) {
  cl_assert(!s_sensor_enabled);
  s_is_watchface = false;

  prv_focus(true);   // a watchapp is focused
  cl_assert(s_sensor_enabled);      // bridge holds the sensor

  prv_focus(false);  // focus lost (modal/exit)
  cl_assert(!s_sensor_enabled);     // released
}

void test_touch_click_synth__watchface_does_not_activate(void) {
  s_is_watchface = true;

  prv_focus(true);   // watchface focused
  cl_assert(!s_sensor_enabled);     // bridge stays inactive

  prv_tap(100, 100, 100);
  cl_assert_equal_i(s_cap_n, 0);    // no synthesis at the watchface
}

// --- tap -> SELECT ----------------------------------------------------------

void test_touch_click_synth__tap_synthesizes_one_select_click(void) {
  prv_focus(true);  // active, no app subscriber

  prv_tap(120, 140, 80 /* ms */);

  // Exactly one SELECT down followed by one SELECT up.
  cl_assert_equal_i(s_cap_n, 2);
  cl_assert_equal_i(s_cap[0].type, PEBBLE_BUTTON_DOWN_EVENT);
  cl_assert_equal_i(s_cap[0].button.button_id, BUTTON_ID_SELECT);
  cl_assert_equal_i(s_cap[1].type, PEBBLE_BUTTON_UP_EVENT);
  cl_assert_equal_i(s_cap[1].button.button_id, BUTTON_ID_SELECT);
}

// --- vertical swipe -> UP / DOWN --------------------------------------------

void test_touch_click_synth__swipe_up_synthesizes_up(void) {
  prv_focus(true);
  prv_swipe(100, 150, 100, 100);  // finger moves up (dy = -50)
  prv_assert_single_click(BUTTON_ID_UP);
}

void test_touch_click_synth__swipe_down_synthesizes_down(void) {
  prv_focus(true);
  prv_swipe(100, 100, 100, 150);  // finger moves down (dy = +50)
  prv_assert_single_click(BUTTON_ID_DOWN);
}

void test_touch_click_synth__horizontal_swipe_ignored(void) {
  prv_focus(true);
  prv_swipe(100, 100, 160, 105);  // predominantly horizontal (dx=60, dy=5)
  cl_assert_equal_i(s_cap_n, 0);  // BACK is a later phase
}

void test_touch_click_synth__swipe_suppressed_by_app_subscriber(void) {
  prv_focus(true);
  prv_app_subscribe();            // app consumes touch itself
  prv_swipe(100, 150, 100, 100);
  cl_assert_equal_i(s_cap_n, 0);  // suppressed -> no double action
}

void test_touch_click_synth__slow_tap_does_not_synthesize(void) {
  prv_focus(true);

  prv_tap(100, 100, 350 /* ms, exceeds the 300ms tap window */);

  cl_assert_equal_i(s_cap_n, 0);
}

// --- arbitration ------------------------------------------------------------

void test_touch_click_synth__app_touch_subscriber_suppresses(void) {
  prv_focus(true);      // bridge active
  prv_app_subscribe();  // app consumes touch itself (ScrollLayer/MenuLayer/etc.)

  prv_tap(100, 100, 80);

  cl_assert_equal_i(s_cap_n, 0);  // suppressed -> no double action
}

// --- gate off ---------------------------------------------------------------

void test_touch_click_synth__inactive_ignores_taps(void) {
  // No focus event -> bridge inactive.
  prv_tap(100, 100, 80);
  cl_assert_equal_i(s_cap_n, 0);
}
