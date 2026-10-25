/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/compositor/compositor_transitions.h>
#include <pbl/util/size.h>

#include <applib/ui/click_internal.h>
#include <applib/ui/recognizer/touch_nav.h>
#include <apps/system/timeline/timeline.h>
#include <apps/system_app_ids.h>
#include <clar.h>
#include <popups/timeline/peek.h>
#include <process_management/app_manager.h>
#include <shell/normal/quick_launch.h>
#include <shell/normal/watchface.h>
#include <shell/normal/watchface_swipe.h>

static AppInstallId s_target;
static ButtonId s_button;
static AppLaunchEventConfig s_launch;
static unsigned s_launch_count;
static Uuid s_pin;
static bool s_first_event;
static bool s_future_empty;
static bool s_transition_future;
static bool s_transition_empty;
static ClickManager *s_clicks;
static AppTimerCallback s_hold_callback;
static void *s_hold_data;
static const CompositorTransition s_slide_transition;
static const CompositorTransition s_peek_transition;

void app_manager_put_launch_app_event(const AppLaunchEventConfig *config) {
  s_launch = *config;
  s_launch_count++;
}

ProcessContext *app_manager_get_task_context(void) {
  static ProcessContext context;
  return &context;
}

bool app_manager_is_watchface_running(void) {
  return true;
}

bool touch_nav_enabled(void) {
  return true;
}

bool quick_launch_swipe_is_enabled(SwipeDirection direction) {
  return true;
}

AppInstallId quick_launch_swipe_get_app(SwipeDirection direction) {
  return s_target;
}

ButtonId touch_nav_swipe_button(SwipeDirection direction) {
  return s_button;
}

void watchface_swipe_init(WatchfaceSwipe *swipe) {
}

void watchface_swipe_cancel(WatchfaceSwipe *swipe) {
}

SwipeDirection watchface_swipe_handle_touch(WatchfaceSwipe *swipe, const TouchEvent *event,
                                            bool eligible) {
  cl_assert(eligible);
  return SwipeDirection_Down;
}

bool quick_launch_is_enabled(ButtonId button) {
  return true;
}

AppInstallId quick_launch_get_app(ButtonId button) {
  return s_target;
}

bool quick_launch_single_click_is_enabled(ButtonId button) {
  return true;
}

AppInstallId quick_launch_single_click_get_app(ButtonId button) {
  return s_target;
}

bool quick_launch_combo_up_down_is_enabled(void) {
  return true;
}

AppInstallId quick_launch_combo_up_down_get_app(void) {
  return s_target;
}

void click_manager_init(ClickManager *manager) {
  s_clicks = manager;
  *manager = (ClickManager){};
  for (ButtonId button = 0; button < NUM_BUTTONS; button++) {
    manager->recognizers[button].button = button;
  }
}

ButtonId click_recognizer_get_button_id(ClickRecognizerRef recognizer) {
  return ((ClickRecognizer *)recognizer)->button;
}

AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void *data) {
  s_hold_callback = callback;
  s_hold_data = data;
  return (AppTimer *)&s_hold_callback;
}

void timeline_peek_get_item_id(TimelineItemId *id) {
  *id = s_pin;
}

bool timeline_peek_is_first_event(void) {
  return s_first_event;
}

bool timeline_peek_is_future_empty(void) {
  return s_future_empty;
}

const CompositorTransition *compositor_slide_transition_timeline_get(bool future, bool destination,
                                                                     bool empty) {
  cl_assert(destination);
  s_transition_future = future;
  s_transition_empty = empty;
  return &s_slide_transition;
}

const CompositorTransition *compositor_peek_transition_timeline_get(void) {
  return &s_peek_transition;
}

bool compositor_is_animating(void) {
  return false;
}

void click_recognizer_reset(ClickRecognizer *recognizer) {
}

void click_manager_reset(ClickManager *manager) {
}

void app_manager_set_minimum_run_level(ProcessAppRunLevel run_level) {
}

void timeline_peek_dismiss(void) {
}

const PebbleProcessMd *quick_launch_setup_get_app_info(void) {
  cl_fail("enabled shortcut should not open setup");
  return nullptr;
}

bool quick_launch_combo_back_up_is_enabled(void) {
  return false;
}

void test_watchface_launch__initialize(void) {
  s_target = APP_ID_TIMELINE_FULL;
  s_button = BUTTON_ID_UP;
  s_launch = (AppLaunchEventConfig){};
  s_launch_count = 0;
  s_pin = UUID_INVALID;
  s_first_event = true;
  s_future_empty = false;
  s_hold_callback = nullptr;
  watchface_init();
}

static void prv_swipe(void) {
  const TouchEvent event = {.type = TouchEvent_Liftoff};
  watchface_handle_touch_event(&event);
}

static void prv_assert_timeline(TimelineDirection direction, AppLaunchReason reason,
                                AppQuickLaunchAction action) {
  cl_assert_equal_i(s_launch.id, s_target);
  cl_assert_equal_i(s_launch.common.reason, reason);
  cl_assert_equal_i(s_launch.common.button, s_button);
  cl_assert_equal_i(s_launch.common.quick_launch_action, action);
  cl_assert(s_launch.common.args != nullptr);
  const TimelineArgs *args = s_launch.common.args;
  cl_assert_equal_i(args->direction, direction);
  cl_assert(args->launch_into_pin);
  cl_assert(args->stay_in_list_view);
  cl_assert(uuid_equal(&args->pin_id, &s_pin));
}

void test_watchface_launch__timeline_swipe_targets_and_buttons(void) {
  const AppInstallId targets[] = {APP_ID_TIMELINE, APP_ID_TIMELINE_PAST, APP_ID_TIMELINE_FULL};
  for (size_t i = 0; i < ARRAY_LENGTH(targets); i++) {
    s_target = targets[i];
    for (s_button = 0; s_button < NUM_BUTTONS; s_button++) {
      prv_swipe();
      const bool future = s_target == APP_ID_TIMELINE ||
                          (s_target == APP_ID_TIMELINE_FULL && s_button != BUTTON_ID_UP);
      prv_assert_timeline(future ? TimelineIterDirectionFuture : TimelineIterDirectionPast,
                          APP_LAUNCH_QUICK_LAUNCH, APP_QUICK_LAUNCH_ACTION_SWIPE);
      cl_assert(s_launch.common.transition == &s_slide_transition);
      cl_assert_equal_b(s_transition_future, future);
    }
  }
  cl_assert_equal_i(s_launch_count, ARRAY_LENGTH(targets) * NUM_BUTTONS);
}

void test_watchface_launch__swipe_preserves_peek_pin_and_transition(void) {
  s_pin = (Uuid){1, 2, 3, 4};
  s_first_event = false;
  prv_swipe();
  prv_assert_timeline(TimelineIterDirectionPast, APP_LAUNCH_QUICK_LAUNCH,
                      APP_QUICK_LAUNCH_ACTION_SWIPE);
  cl_assert(s_launch.common.transition == &s_peek_transition);

  s_first_event = true;
  s_future_empty = true;
  prv_swipe();
  prv_assert_timeline(TimelineIterDirectionPast, APP_LAUNCH_QUICK_LAUNCH,
                      APP_QUICK_LAUNCH_ACTION_SWIPE);
  cl_assert(s_launch.common.transition == &s_slide_transition);
  cl_assert(s_transition_empty);
}

void test_watchface_launch__timeline_tap_and_hold(void) {
  ClickRecognizer *recognizer = &s_clicks->recognizers[s_button];
  recognizer->config.click.handler(recognizer, nullptr);
  prv_assert_timeline(TimelineIterDirectionPast, APP_LAUNCH_SYSTEM, APP_QUICK_LAUNCH_ACTION_TAP);
  recognizer->config.long_click.handler(recognizer, nullptr);
  prv_assert_timeline(TimelineIterDirectionPast, APP_LAUNCH_QUICK_LAUNCH,
                      APP_QUICK_LAUNCH_ACTION_HOLD);
}

void test_watchface_launch__timeline_combo_keeps_future_direction(void) {
  PebbleEvent event = {.type = PEBBLE_BUTTON_DOWN_EVENT, .button.button_id = BUTTON_ID_UP};
  watchface_handle_button_event(&event);
  event.button.button_id = BUTTON_ID_DOWN;
  watchface_handle_button_event(&event);
  cl_assert(s_hold_callback != nullptr);
  s_hold_callback(s_hold_data);
  prv_assert_timeline(TimelineIterDirectionFuture, APP_LAUNCH_QUICK_LAUNCH,
                      APP_QUICK_LAUNCH_ACTION_COMBO);
  cl_assert(s_transition_future);
}

void test_watchface_launch__ordinary_swipe_metadata(void) {
  s_target = 123;
  prv_swipe();
  cl_assert_equal_i(s_launch.id, s_target);
  cl_assert_equal_i(s_launch.common.reason, APP_LAUNCH_QUICK_LAUNCH);
  cl_assert_equal_i(s_launch.common.button, s_button);
  cl_assert_equal_i((uintptr_t)s_launch.common.args, APP_QUICK_LAUNCH_ACTION_SWIPE);
}

void test_watchface_launch__invalid_and_nothing_do_not_launch(void) {
  s_target = INSTALL_ID_INVALID;
  prv_swipe();
  s_target = APP_ID_QUICK_LAUNCH_NOTHING;
  prv_swipe();
  cl_assert_equal_i(s_launch_count, 0);
}
