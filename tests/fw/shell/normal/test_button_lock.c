/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "shell/normal/button_lock.h"

#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/dialogs/simple_dialog.h"
#include "kernel/events.h"
#include "kernel/ui/modals/modal_manager.h"
#include "resource/resource_ids.auto.h"
#include "pbl/services/battery/battery_state.h"
#include "shell/prefs.h"

#include "clar.h"

// Stubs
///////////////////////////////////////////////////////////////////////////////
#include "stubs_logging.h"
#include "stubs_passert.h"

#include "fake_new_timer.h"

// Fakes
///////////////////////////////////////////////////////////////////////////////

static uint32_t s_pref_hold_ms;

uint32_t shell_prefs_get_button_lock_hold_ms(void) {
  return s_pref_hold_ms;
}

static uint32_t s_pref_auto_ms;
static bool s_pref_auto_paused;
static ButtonLockAutoScope s_pref_auto_scope;
static bool s_pref_auto_not_charging;

uint32_t shell_prefs_get_button_lock_auto_ms(void) {
  return s_pref_auto_ms;
}

bool shell_prefs_get_button_lock_auto_paused(void) {
  return s_pref_auto_paused;
}

ButtonLockAutoScope shell_prefs_get_button_lock_auto_scope(void) {
  return s_pref_auto_scope;
}

bool shell_prefs_get_button_lock_auto_not_charging(void) {
  return s_pref_auto_not_charging;
}

static bool s_is_plugged;

BatteryChargeState battery_get_charge_state(void) {
  return (BatteryChargeState){.is_plugged = s_is_plugged};
}

static bool s_workout_ongoing;

bool workout_service_is_workout_ongoing(void) {
  return s_workout_ongoing;
}

static bool s_modal_enabled;
static ModalProperty s_modal_properties = ModalPropertyDefault;

bool modal_manager_get_enabled(void) {
  return s_modal_enabled;
}

ModalProperty modal_manager_get_properties(void) {
  return s_modal_properties;
}

static ModalPriority s_modal_top_focused_priority = ModalPriorityInvalid;

ModalPriority modal_manager_get_top_focused_priority(void) {
  return s_modal_enabled ? s_modal_top_focused_priority : ModalPriorityInvalid;
}

static bool s_watchface_running;

bool app_manager_is_watchface_running(void) {
  return s_watchface_running;
}

static int s_num_cancel_force_quit_calls;

void launcher_cancel_force_quit(void) {
  s_num_cancel_force_quit_calls++;
}

static int s_num_watchface_reset_calls;

void watchface_reset_click_manager(void) {
  s_num_watchface_reset_calls++;
}

static CallbackEventCallback s_kernel_cb;
static void *s_kernel_cb_data;

void launcher_task_add_callback(CallbackEventCallback callback, void *data) {
  s_kernel_cb = callback;
  s_kernel_cb_data = data;
}

static bool s_touch_enabled = true;
static bool s_touch_pref_enabled = true;

void touch_service_set_globally_enabled(bool enabled) {
  s_touch_enabled = enabled;
}

bool touch_is_globally_enabled(void) {
  return s_touch_pref_enabled;
}

static int s_num_short_pulses;
static int s_num_double_pulses;

void vibes_short_pulse(void) {
  s_num_short_pulses++;
}

void vibes_double_pulse(void) {
  s_num_double_pulses++;
}

static int s_num_dialogs_created;
static int s_num_dialogs_popped;
static SimpleDialog s_dialog_storage;
static DialogCallbacks s_dialog_callbacks;
static const char *s_last_dialog_text;

SimpleDialog *simple_dialog_create(const char *dialog_name) {
  s_num_dialogs_created++;
  return &s_dialog_storage;
}

Dialog *simple_dialog_get_dialog(SimpleDialog *simple_dialog) {
  return &simple_dialog->dialog;
}

void dialog_set_text(Dialog *dialog, const char *text) {
  s_last_dialog_text = text;
}

void dialog_set_icon(Dialog *dialog, uint32_t icon_id) {
}

void dialog_set_timeout(Dialog *dialog, uint32_t timeout) {
}

void dialog_set_callbacks(Dialog *dialog, const DialogCallbacks *callbacks,
                          void *callback_context) {
  s_dialog_callbacks = *callbacks;
}

void simple_dialog_push(SimpleDialog *simple_dialog, WindowStack *window_stack) {
}

void dialog_pop(Dialog *dialog) {
  s_num_dialogs_popped++;
}

static ModalPriority s_last_dialog_priority;

WindowStack *modal_manager_get_window_stack(ModalPriority priority) {
  s_last_dialog_priority = priority;
  return NULL;
}

const char *i18n_get(const char *string, const void *owner) {
  return string;
}

void i18n_free(const char *string, const void *owner) {
}

// Helpers
///////////////////////////////////////////////////////////////////////////////

static bool prv_press(ButtonId id) {
  PebbleEvent e = {
    .type = PEBBLE_BUTTON_DOWN_EVENT,
    .button.button_id = id,
  };
  return button_lock_handle_button_event(&e);
}

static bool prv_release(ButtonId id) {
  PebbleEvent e = {
    .type = PEBBLE_BUTTON_UP_EVENT,
    .button.button_id = id,
  };
  return button_lock_handle_button_event(&e);
}

//! button_lock_init creates the combo timer first and the auto-lock timer
//! second, and this test is the only thing creating timers.
#define COMBO_TIMER_ID (1)
#define AUTO_TIMER_ID  (2)

static void prv_invoke_kernel_cb(void) {
  cl_assert(s_kernel_cb != NULL);
  CallbackEventCallback cb = s_kernel_cb;
  s_kernel_cb = NULL;
  cb(s_kernel_cb_data);
}

//! Hold the combo, fire the hold timer and run the posted KernelMain callback.
static void prv_toggle_lock(void) {
  prv_press(BUTTON_ID_BACK);
  prv_press(BUTTON_ID_DOWN);
  stub_new_timer_fire(COMBO_TIMER_ID);
  prv_invoke_kernel_cb();
  prv_release(BUTTON_ID_BACK);
  prv_release(BUTTON_ID_DOWN);
}

//! A button press and release, which is all auto-lock needs to see to restart.
static void prv_activity(void) {
  prv_press(BUTTON_ID_SELECT);
  prv_release(BUTTON_ID_SELECT);
}

//! Let the idle timer expire and run the posted KernelMain callback.
static void prv_expire_auto_lock(void) {
  cl_assert(stub_new_timer_is_scheduled(AUTO_TIMER_ID));
  stub_new_timer_fire(AUTO_TIMER_ID);
  prv_invoke_kernel_cb();
}

// Tests
///////////////////////////////////////////////////////////////////////////////

void test_button_lock__initialize(void) {
  static bool s_timer_created;
  if (!s_timer_created) {
    button_lock_init();
    s_timer_created = true;
  }

  s_pref_hold_ms = 2000;
  s_pref_auto_ms = 0;
  s_pref_auto_paused = false;
  s_pref_auto_scope = ButtonLockAutoScopeBoth;
  s_pref_auto_not_charging = true;
  s_is_plugged = false;
  s_workout_ongoing = false;
  s_modal_enabled = false;
  s_modal_properties = ModalPropertyDefault;
  s_watchface_running = true;

  // Unwind state a previous test may have left behind.
  for (ButtonId id = 0; id < NUM_BUTTONS; id++) {
    prv_release(id);
  }
  if (button_lock_is_locked()) {
    prv_toggle_lock();
  }

  s_num_cancel_force_quit_calls = 0;
  s_num_watchface_reset_calls = 0;
  s_kernel_cb = NULL;
  s_touch_enabled = true;
  s_touch_pref_enabled = true;
  s_num_short_pulses = 0;
  s_num_double_pulses = 0;
  s_num_dialogs_created = 0;
  s_num_dialogs_popped = 0;
  s_last_dialog_text = NULL;
  s_last_dialog_priority = ModalPriorityInvalid;
  s_modal_top_focused_priority = ModalPriorityInvalid;
}

void test_button_lock__cleanup(void) {
}

void test_button_lock__pref_disabled_is_inert(void) {
  s_pref_hold_ms = 0;
  const int num_timer_starts_before = s_num_new_timer_start_calls;

  cl_assert(!prv_press(BUTTON_ID_BACK));
  cl_assert(!prv_press(BUTTON_ID_DOWN));
  cl_assert_equal_i(s_num_new_timer_start_calls, num_timer_starts_before);
  cl_assert(!prv_release(BUTTON_ID_BACK));
  cl_assert(!prv_release(BUTTON_ID_DOWN));
  cl_assert(!button_lock_is_locked());
}

void test_button_lock__lock_engages_after_hold(void) {
  cl_assert(!prv_press(BUTTON_ID_BACK));
  cl_assert(prv_press(BUTTON_ID_DOWN));

  cl_assert_equal_i(s_num_cancel_force_quit_calls, 1);
  cl_assert_equal_i(s_num_watchface_reset_calls, 1);
  TimerID timer = stub_new_timer_get_next();
  cl_assert(stub_new_timer_is_scheduled(timer));
  cl_assert_equal_i(stub_new_timer_timeout(timer), 2000);

  stub_new_timer_invoke(1);
  prv_invoke_kernel_cb();

  cl_assert(button_lock_is_locked());
  cl_assert_equal_i(s_num_short_pulses, 1);
  cl_assert(!s_touch_enabled);
  cl_assert_equal_s(s_last_dialog_text, "Buttons Locked");
  // Popups must outrank notifications/calls, but stay below pairing/alarms.
  cl_assert_equal_i(s_last_dialog_priority, ModalPriorityAlert);

  // The first button's DOWN was delivered, so its UP must be too.
  cl_assert(!prv_release(BUTTON_ID_BACK));
  cl_assert(prv_release(BUTTON_ID_DOWN));
}

void test_button_lock__configurable_hold_duration(void) {
  s_pref_hold_ms = 5000;

  prv_press(BUTTON_ID_BACK);
  prv_press(BUTTON_ID_DOWN);
  cl_assert_equal_i(stub_new_timer_timeout(stub_new_timer_get_next()), 5000);
  prv_release(BUTTON_ID_BACK);
  prv_release(BUTTON_ID_DOWN);
}

void test_button_lock__release_before_timeout_aborts(void) {
  prv_press(BUTTON_ID_BACK);
  prv_press(BUTTON_ID_DOWN);
  TimerID timer = stub_new_timer_get_next();

  cl_assert(!prv_release(BUTTON_ID_BACK));
  cl_assert(!stub_new_timer_is_scheduled(timer));
  // The second button's DOWN was swallowed, so its UP must be too.
  cl_assert(prv_release(BUTTON_ID_DOWN));
  cl_assert(!button_lock_is_locked());
}

void test_button_lock__third_button_cancels_pending(void) {
  prv_press(BUTTON_ID_BACK);
  prv_press(BUTTON_ID_DOWN);
  TimerID timer = stub_new_timer_get_next();

  cl_assert(!prv_press(BUTTON_ID_SELECT));
  cl_assert(!stub_new_timer_is_scheduled(timer));

  prv_release(BUTTON_ID_SELECT);
  prv_release(BUTTON_ID_BACK);
  prv_release(BUTTON_ID_DOWN);
  cl_assert(!button_lock_is_locked());
}

void test_button_lock__locked_swallows_input_and_hints(void) {
  prv_toggle_lock();
  cl_assert(button_lock_is_locked());

  s_num_dialogs_created = 0;
  cl_assert(prv_press(BUTTON_ID_SELECT));
  cl_assert(prv_release(BUTTON_ID_SELECT));
  cl_assert_equal_i(s_num_dialogs_created, 1);
  cl_assert_equal_s(s_last_dialog_text, "Hold Back + Down to unlock");

  // Hint popup is not re-created while still on screen.
  cl_assert(prv_press(BUTTON_ID_UP));
  cl_assert(prv_release(BUTTON_ID_UP));
  cl_assert_equal_i(s_num_dialogs_created, 1);

  // Once it unloaded, another press shows it again.
  s_dialog_callbacks.unload(NULL);
  cl_assert(prv_press(BUTTON_ID_UP));
  cl_assert(prv_release(BUTTON_ID_UP));
  cl_assert_equal_i(s_num_dialogs_created, 2);
}

void test_button_lock__hint_rises_above_an_interrupting_modal(void) {
  prv_toggle_lock();

  // An alarm would render over a hint pushed at ModalPriorityAlert, so the hint
  // goes onto the alarm's own stack instead.
  s_modal_enabled = true;
  s_modal_properties = ModalProperty_Exists;
  s_modal_top_focused_priority = ModalPriorityAlarm;

  cl_assert(prv_press(BUTTON_ID_SELECT));
  cl_assert(prv_release(BUTTON_ID_SELECT));
  cl_assert_equal_s(s_last_dialog_text, "Hold Back + Down to unlock");
  cl_assert_equal_i(s_last_dialog_priority, ModalPriorityAlarm);
}

void test_button_lock__hint_stays_at_alert_below_alert_modals(void) {
  prv_toggle_lock();

  // An incoming call is below Alert, where the hint is already visible.
  s_modal_enabled = true;
  s_modal_properties = ModalProperty_Exists;
  s_modal_top_focused_priority = ModalPriorityPhone;

  cl_assert(prv_press(BUTTON_ID_SELECT));
  cl_assert(prv_release(BUTTON_ID_SELECT));
  cl_assert_equal_i(s_last_dialog_priority, ModalPriorityAlert);
}

void test_button_lock__lock_toasts_stay_at_alert(void) {
  s_modal_enabled = true;
  s_modal_properties = ModalProperty_Exists;
  s_modal_top_focused_priority = ModalPriorityAlarm;

  // The lock/unlock feedback must never cover an alarm.
  prv_toggle_lock();
  cl_assert(button_lock_is_locked());
  cl_assert_equal_s(s_last_dialog_text, "Buttons Locked");
  cl_assert_equal_i(s_last_dialog_priority, ModalPriorityAlert);

  prv_toggle_lock();
  cl_assert(!button_lock_is_locked());
  cl_assert_equal_s(s_last_dialog_text, "Buttons Unlocked");
  cl_assert_equal_i(s_last_dialog_priority, ModalPriorityAlert);
}

void test_button_lock__unlock_restores_touch_pref(void) {
  prv_toggle_lock();
  cl_assert(button_lock_is_locked());
  cl_assert(!s_touch_enabled);

  s_touch_pref_enabled = false;
  prv_toggle_lock();
  cl_assert(!button_lock_is_locked());
  cl_assert_equal_i(s_num_double_pulses, 1);
  // Touch comes back to the persisted pref, not blindly on.
  cl_assert(!s_touch_enabled);

  prv_toggle_lock();
  s_touch_pref_enabled = true;
  prv_toggle_lock();
  cl_assert(s_touch_enabled);
}

void test_button_lock__continuous_hold_toggles_once(void) {
  prv_press(BUTTON_ID_BACK);
  prv_press(BUTTON_ID_DOWN);
  stub_new_timer_invoke(1);
  prv_invoke_kernel_cb();
  cl_assert(button_lock_is_locked());

  // Still holding: the timer must not be re-armed.
  cl_assert(!stub_new_timer_is_scheduled(stub_new_timer_get_next()));

  prv_release(BUTTON_ID_BACK);
  prv_release(BUTTON_ID_DOWN);
  cl_assert(button_lock_is_locked());
}

void test_button_lock__no_watchface_reset_in_app(void) {
  s_watchface_running = false;

  prv_press(BUTTON_ID_BACK);
  prv_press(BUTTON_ID_DOWN);
  cl_assert_equal_i(s_num_watchface_reset_calls, 0);
  cl_assert_equal_i(s_num_cancel_force_quit_calls, 1);
  prv_release(BUTTON_ID_BACK);
  prv_release(BUTTON_ID_DOWN);
}

void test_button_lock__timer_fire_after_release_race(void) {
  prv_press(BUTTON_ID_BACK);
  prv_press(BUTTON_ID_DOWN);

  // Timer fires, but the combo is released before KernelMain runs the
  // posted callback: the toggle must not happen.
  stub_new_timer_invoke(1);
  prv_release(BUTTON_ID_BACK);
  prv_release(BUTTON_ID_DOWN);
  prv_invoke_kernel_cb();

  cl_assert(!button_lock_is_locked());
}

void test_button_lock__unlock_while_hint_visible_pops_it(void) {
  prv_toggle_lock();

  s_num_dialogs_created = 0;
  prv_press(BUTTON_ID_SELECT);
  prv_release(BUTTON_ID_SELECT);
  cl_assert_equal_i(s_num_dialogs_created, 1);

  s_num_dialogs_popped = 0;
  prv_toggle_lock();
  cl_assert(!button_lock_is_locked());
  cl_assert_equal_i(s_num_dialogs_popped, 1);
  cl_assert_equal_s(s_last_dialog_text, "Buttons Unlocked");
}

// Auto-lock
///////////////////////////////////////////////////////////////////////////////

void test_button_lock__auto_lock_off_by_default(void) {
  prv_activity();
  cl_assert(!stub_new_timer_is_scheduled(AUTO_TIMER_ID));
}

void test_button_lock__auto_lock_engages_after_idle(void) {
  s_pref_auto_ms = 30000;
  prv_activity();
  cl_assert_equal_i(stub_new_timer_timeout(AUTO_TIMER_ID), 30000);

  prv_expire_auto_lock();

  cl_assert(button_lock_is_locked());
  cl_assert_equal_i(s_num_short_pulses, 1);
  cl_assert(!s_touch_enabled);
  cl_assert_equal_s(s_last_dialog_text, "Buttons Locked");
}

void test_button_lock__auto_lock_needs_the_unlock_combo(void) {
  s_pref_hold_ms = 0;
  s_pref_auto_ms = 30000;
  prv_activity();
  cl_assert(!stub_new_timer_is_scheduled(AUTO_TIMER_ID));
}

void test_button_lock__auto_lock_respects_pause(void) {
  s_pref_auto_ms = 30000;
  s_pref_auto_paused = true;
  prv_activity();
  cl_assert(!stub_new_timer_is_scheduled(AUTO_TIMER_ID));
}

void test_button_lock__auto_lock_disarmed_until_first_activity(void) {
  s_pref_auto_ms = 30000;
  button_lock_disarm_auto_lock_for_test();
  cl_assert(!stub_new_timer_is_scheduled(AUTO_TIMER_ID));

  // Nothing but real activity may arm it, so a charger change must not either.
  button_lock_handle_charger_change(false /* is_plugged */);
  cl_assert(!stub_new_timer_is_scheduled(AUTO_TIMER_ID));

  prv_activity();
  cl_assert(stub_new_timer_is_scheduled(AUTO_TIMER_ID));
}

void test_button_lock__auto_lock_postponed_while_charging(void) {
  s_pref_auto_ms = 30000;
  prv_activity();

  s_is_plugged = true;
  prv_expire_auto_lock();

  cl_assert(!button_lock_is_locked());
  // Postponed, not cancelled: the cable can go away without any activity.
  cl_assert(stub_new_timer_is_scheduled(AUTO_TIMER_ID));

  s_is_plugged = false;
  prv_expire_auto_lock();
  cl_assert(button_lock_is_locked());
}

void test_button_lock__charging_does_not_release_an_engaged_lock(void) {
  s_pref_auto_ms = 30000;
  prv_activity();
  prv_expire_auto_lock();
  cl_assert(button_lock_is_locked());

  button_lock_handle_charger_change(true /* is_plugged */);
  cl_assert(button_lock_is_locked());
}

void test_button_lock__auto_lock_ignores_charger_when_pref_off(void) {
  s_pref_auto_ms = 30000;
  s_pref_auto_not_charging = false;
  s_is_plugged = true;
  prv_activity();

  prv_expire_auto_lock();
  cl_assert(button_lock_is_locked());
}

void test_button_lock__auto_lock_postponed_while_modal_focused(void) {
  s_pref_auto_ms = 30000;
  prv_activity();

  // An alarm or an incoming call must stay dismissable.
  s_modal_enabled = true;
  s_modal_properties = ModalProperty_Exists;
  prv_expire_auto_lock();

  cl_assert(!button_lock_is_locked());
  cl_assert(stub_new_timer_is_scheduled(AUTO_TIMER_ID));
}

void test_button_lock__auto_lock_scope_general_use(void) {
  s_pref_auto_ms = 30000;
  s_pref_auto_scope = ButtonLockAutoScopeGeneralUse;
  s_workout_ongoing = true;
  prv_activity();

  prv_expire_auto_lock();
  cl_assert(!button_lock_is_locked());

  s_workout_ongoing = false;
  prv_expire_auto_lock();
  cl_assert(button_lock_is_locked());
}

void test_button_lock__auto_lock_scope_during_activity(void) {
  s_pref_auto_ms = 30000;
  s_pref_auto_scope = ButtonLockAutoScopeDuringActivity;
  prv_activity();

  prv_expire_auto_lock();
  cl_assert(!button_lock_is_locked());

  s_workout_ongoing = true;
  prv_expire_auto_lock();
  cl_assert(button_lock_is_locked());
}

void test_button_lock__auto_lock_idle_while_locked(void) {
  s_pref_auto_ms = 30000;
  prv_activity();
  prv_toggle_lock();
  cl_assert(button_lock_is_locked());
  cl_assert(!stub_new_timer_is_scheduled(AUTO_TIMER_ID));

  // Buttons pressed while locked must not re-arm it either.
  prv_press(BUTTON_ID_SELECT);
  prv_release(BUTTON_ID_SELECT);
  cl_assert(!stub_new_timer_is_scheduled(AUTO_TIMER_ID));
}

void test_button_lock__auto_lock_rearms_after_unlock(void) {
  s_pref_auto_ms = 30000;
  prv_activity();
  prv_expire_auto_lock();
  cl_assert(button_lock_is_locked());

  prv_toggle_lock();
  cl_assert(!button_lock_is_locked());
  cl_assert(stub_new_timer_is_scheduled(AUTO_TIMER_ID));
}

void test_button_lock__touch_activity_restarts_the_countdown(void) {
  s_pref_auto_ms = 30000;
  prv_activity();
  stub_new_timer_stop(AUTO_TIMER_ID);

  button_lock_handle_activity();
  cl_assert(stub_new_timer_is_scheduled(AUTO_TIMER_ID));
}

void test_button_lock__disabling_the_combo_releases_the_lock(void) {
  prv_toggle_lock();
  cl_assert(button_lock_is_locked());

  // Otherwise nothing could release it: the combo is the only way out.
  s_pref_hold_ms = 0;
  button_lock_handle_prefs_changed();
  prv_invoke_kernel_cb();

  cl_assert(!button_lock_is_locked());
  cl_assert(s_touch_enabled);
}
