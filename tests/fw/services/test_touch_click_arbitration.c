/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Verification for the proposed touch->button "synthesis bridge" arbitration.
//
// This test does NOT implement the synthesis bridge. It models the bridge's
// suppression *decision* (the proposal: synthesize a button only when the
// focused app is NOT a direct touch subscriber) against the REAL touch service
// bookkeeping (touch_has_app_subscribers()), plus a dummy "app touch handler"
// standing in for ScrollLayer/MenuLayer or a custom touch_service subscriber.
//
// Goal: show that with the arbitration in place, a single touch never produces
// a double action (app touch handling AND a synthesized button), and to make
// the async-subscription timing window observable.
//
// Note on isolation: touch.c's subscriber count is a module static that
// touch_init() does not reset. In the firmware it is drained on app exit via
// event_service_clear_process_subscriptions(); here the test drains it in
// cleanup so each case starts from zero subscribers.

#include "clar.h"

#include "kernel/events.h"
#include "kernel/pebble_tasks.h"
#include "pbl/services/event_service.h"
#include "pbl/services/touch/touch.h"
#include "pbl/services/touch/touch_event.h"

#include <stdbool.h>
#include <stdint.h>

#include "fake_events.h"

// Stubs (same set as test_touch.c)
#include "stubs_analytics.h"
#include "stubs_logging.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"

void kernel_free(void *p) {}

// Capture the touch service's subscriber add/remove callbacks. In the real
// system these fire on KernelMain when it processes a PEBBLE_SUBSCRIPTION_EVENT
// (i.e. app-task subscribe/unsubscribe is asynchronous). Calling them here
// models that kernel-side processing point exactly.
static EventServiceAddSubscriberCallback s_add_subscriber_cb;
static EventServiceRemoveSubscriberCallback s_remove_subscriber_cb;

void event_service_init(PebbleEventType type, EventServiceAddSubscriberCallback add_cb,
                        EventServiceRemoveSubscriberCallback remove_cb) {
  cl_assert(type == PEBBLE_TOUCH_EVENT || type == PEBBLE_GESTURE_EVENT);
  s_add_subscriber_cb = add_cb;
  s_remove_subscriber_cb = remove_cb;
}

static bool s_touch_sensor_enabled;
void touch_sensor_set_enabled(bool enabled) { s_touch_sensor_enabled = enabled; }

//////////////////////////////////////////////////////////////////////////////
// Test-side subscriber bookkeeping (drains leaks in cleanup).

static int s_net_app_subs;

static void prv_app_subscribe(void) {
  s_add_subscriber_cb(PebbleTask_App);  // models KernelMain processing the sub
  s_net_app_subs++;
}

static void prv_app_unsubscribe(void) {
  s_remove_subscriber_cb(PebbleTask_App);
  s_net_app_subs--;
}

//////////////////////////////////////////////////////////////////////////////
// Models used only by this test (NOT the real bridge / app)

// The proposed arbitration decision the synthesis bridge would make.
static bool prv_bridge_should_synthesize(void) {
  return !touch_has_app_subscribers();
}

static int s_synthesized_buttons;  // buttons the bridge would emit
static int s_app_touch_actions;    // actions the dummy app touch handler took

// Deliver a single touch gesture to BOTH would-be consumers, exactly as the
// real system would: the kernel synthesis bridge and the app's own touch
// handler both observe the same PEBBLE_TOUCH_EVENT. `app_handles_touch` models
// whether the focused app actually consumes touch (ScrollLayer/MenuLayer or a
// custom subscriber). It is kept independent from the subscriber count so we
// can also exercise the "partial coverage" and timing-window cases.
static void prv_deliver_gesture(bool app_handles_touch) {
  if (prv_bridge_should_synthesize()) {
    s_synthesized_buttons++;
  }
  if (app_handles_touch) {
    s_app_touch_actions++;
  }
}

static int prv_total_actions(void) {
  return s_synthesized_buttons + s_app_touch_actions;
}

//////////////////////////////////////////////////////////////////////////////

void test_touch_click_arbitration__initialize(void) {
  fake_event_init();
  s_add_subscriber_cb = NULL;
  s_remove_subscriber_cb = NULL;
  s_touch_sensor_enabled = false;
  s_net_app_subs = 0;
  s_synthesized_buttons = 0;
  s_app_touch_actions = 0;
  touch_init();
  touch_reset();
  touch_service_set_globally_enabled(true);
}

void test_touch_click_arbitration__cleanup(void) {
  // Drain any subscribers the test left registered so the module-static count
  // in touch.c doesn't leak into the next test.
  while (s_net_app_subs > 0) {
    prv_app_unsubscribe();
  }
}

// Baseline: a button-only app (no touch subscriber) gets button emulation, and
// there is exactly one action (the synthesized button).
void test_touch_click_arbitration__button_only_app_gets_synthesis(void) {
  cl_assert(!touch_has_app_subscribers());

  prv_deliver_gesture(false /* app does not handle touch */);

  cl_assert_equal_i(s_synthesized_buttons, 1);
  cl_assert_equal_i(s_app_touch_actions, 0);
  cl_assert_equal_i(prv_total_actions(), 1);  // no double action
}

// A touch-consuming app (ScrollLayer/MenuLayer or custom subscriber) suppresses
// synthesis: exactly one action (the app's), never a double.
void test_touch_click_arbitration__touch_app_suppresses_synthesis(void) {
  // App subscribes in window_load; the kernel processes it (this callback).
  prv_app_subscribe();
  cl_assert(touch_has_app_subscribers());

  prv_deliver_gesture(true /* app handles touch */);

  cl_assert_equal_i(s_synthesized_buttons, 0);  // suppressed
  cl_assert_equal_i(s_app_touch_actions, 1);
  cl_assert_equal_i(prv_total_actions(), 1);  // no double action
}

// Q2: dynamic subscribe/unsubscribe within a window (e.g. a screen that only
// uses touch sometimes). Suppression must track the current subscription.
void test_touch_click_arbitration__dynamic_subscribe_unsubscribe_tracks(void) {
  // Phase 1: not subscribed -> button emulation.
  prv_deliver_gesture(false);
  cl_assert_equal_i(s_synthesized_buttons, 1);
  cl_assert_equal_i(s_app_touch_actions, 0);

  // Phase 2: app subscribes -> synthesis suppressed, app handles.
  prv_app_subscribe();
  prv_deliver_gesture(true);
  cl_assert_equal_i(s_synthesized_buttons, 1);  // unchanged
  cl_assert_equal_i(s_app_touch_actions, 1);

  // Phase 3: app unsubscribes -> button emulation resumes.
  prv_app_unsubscribe();
  cl_assert(!touch_has_app_subscribers());
  prv_deliver_gesture(false);
  cl_assert_equal_i(s_synthesized_buttons, 2);
  cl_assert_equal_i(s_app_touch_actions, 1);

  // Every gesture produced exactly one action across all phases.
  cl_assert_equal_i(prv_total_actions(), 3);
}

// Q1 (no race with the *coupled* sensor design): the sensor is powered only
// while a subscriber exists, and the enable happens in the same callback that
// increments the count. So no touch event can be delivered before the count
// reflects the subscription -> the first touch after subscribe already sees the
// subscriber and is suppressed.
void test_touch_click_arbitration__coupled_sensor_is_race_free(void) {
  cl_assert(!s_touch_sensor_enabled);

  // App subscribes: sensor comes up AND count flips together.
  prv_app_subscribe();
  cl_assert(s_touch_sensor_enabled);
  cl_assert(touch_has_app_subscribers());

  // Only now can touch events flow; the very first one is already suppressed.
  prv_deliver_gesture(true);
  cl_assert_equal_i(s_synthesized_buttons, 0);
  cl_assert_equal_i(prv_total_actions(), 1);
}

// Q1 (the race that the *persistent-sensor* bridge design would introduce): if
// the bridge keeps the sensor on globally, touch events can arrive in the small
// window after the app calls subscribe() but before KernelMain processes the
// PEBBLE_SUBSCRIPTION_EVENT (count not yet incremented). This test makes that
// window observable: a gesture in that window is (wrongly) synthesized.
void test_touch_click_arbitration__persistent_sensor_has_timing_window(void) {
  // Persistent-sensor bridge: touch flows regardless of app subscription.
  // Model the window: the app has *called* subscribe(), but the kernel has not
  // yet run the add-subscriber callback, so the count is still 0.
  cl_assert(!touch_has_app_subscribers());

  prv_deliver_gesture(false /* app not yet wired up to handle */);
  // The bridge synthesized a button even though the app is about to own touch.
  cl_assert_equal_i(s_synthesized_buttons, 1);

  // Kernel now processes the subscription; subsequent gestures are suppressed.
  prv_app_subscribe();
  prv_deliver_gesture(true);
  cl_assert_equal_i(s_synthesized_buttons, 1);  // no further synthesis
  cl_assert_equal_i(s_app_touch_actions, 1);
}

// Q3 (partial coverage is conservative/safe): a window that BOTH subscribes to
// touch and uses button click config. Any touch subscription suppresses ALL
// synthesis for that app -> the click config gets no touch emulation, but there
// is never a double action. Suppression errs toward "do nothing", not misfire.
void test_touch_click_arbitration__partial_coverage_never_double_fires(void) {
  prv_app_subscribe();  // window has a touch subscriber

  // Gesture that the app's touch handler consumes: no synthesized button even
  // though the same window also has button handlers installed.
  prv_deliver_gesture(true);
  cl_assert_equal_i(s_synthesized_buttons, 0);
  cl_assert_equal_i(s_app_touch_actions, 1);
  cl_assert_equal_i(prv_total_actions(), 1);
}

// The backlight touch-wake subscription must NOT be mistaken for an app
// subscriber, otherwise the bridge would suppress itself whenever the backlight
// feature is on. Confirms the arbitration key ignores the backlight holder.
void test_touch_click_arbitration__backlight_does_not_suppress(void) {
  touch_set_backlight_enabled(true);
  cl_assert(!touch_has_app_subscribers());

  prv_deliver_gesture(false);
  cl_assert_equal_i(s_synthesized_buttons, 1);  // still emulated

  touch_set_backlight_enabled(false);
}
