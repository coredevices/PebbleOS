/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/services/battery/battery_state.h>

#include <applib/ui/dialogs/dialog.h>
#include <applib/ui/dialogs/simple_dialog.h>
#include <clar.h>
#include <kernel/events.h>
#include <kernel/ui/modals/modal_manager.h>
#include <resource/resource_ids.auto.h>
#include <shell/normal/button_lock.h>
#include <shell/normal/quick_launch.h>
#include <shell/prefs.h>
#include <shell/system_app_ids.auto.h>

// Stubs
///////////////////////////////////////////////////////////////////////////////
#include <fake_new_timer.h>
#include <stubs_logging.h>
#include <stubs_passert.h>

// Fakes
///////////////////////////////////////////////////////////////////////////////

static uint32_t s_pref_hold_ms;

uint32_t shell_prefs_get_button_lock_hold_ms(void) {
  return s_pref_hold_ms;
}

static ButtonLockCombo s_pref_combo;

ButtonLockCombo shell_prefs_get_button_lock_combo(void) {
  return s_pref_combo;
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

//! Quick Launch bindings: the app bound to each hold gesture and combo, and
//! whether that slot is enabled.
static AppInstallId s_ql_hold_app[NUM_BUTTONS];
static bool s_ql_hold_enabled[NUM_BUTTONS];
static AppInstallId s_ql_back_up_app;
static bool s_ql_back_up_enabled;
static AppInstallId s_ql_up_down_app;
static bool s_ql_up_down_enabled;

bool quick_launch_is_enabled(ButtonId button) {
  return s_ql_hold_enabled[button];
}

AppInstallId quick_launch_get_app(ButtonId button) {
  return s_ql_hold_app[button];
}

bool quick_launch_combo_back_up_is_enabled(void) {
  return s_ql_back_up_enabled;
}

AppInstallId quick_launch_combo_back_up_get_app(void) {
  return s_ql_back_up_app;
}

bool quick_launch_combo_up_down_is_enabled(void) {
  return s_ql_up_down_enabled;
}

AppInstallId quick_launch_combo_up_down_get_app(void) {
  return s_ql_up_down_app;
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

//! button_lock_init creates the hold timer first and the auto-lock timer
//! second, and this test is the only thing creating timers.
#define HOLD_TIMER_ID (1)
#define AUTO_TIMER_ID (2)

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

static void prv_bind_hold(ButtonId id) {
  s_ql_hold_app[id] = APP_ID_BUTTON_LOCK_TOGGLE;
  s_ql_hold_enabled[id] = true;
}

static void prv_unbind_all(void) {
  for (ButtonId id = 0; id < NUM_BUTTONS; id++) {
    s_ql_hold_app[id] = INSTALL_ID_INVALID;
    s_ql_hold_enabled[id] = false;
  }
  s_ql_back_up_app = INSTALL_ID_INVALID;
  s_ql_back_up_enabled = false;
  s_ql_up_down_app = INSTALL_ID_INVALID;
  s_ql_up_down_enabled = false;
}

static void prv_invoke_kernel_cb(void) {
  cl_assert(s_kernel_cb != NULL);
  CallbackEventCallback cb = s_kernel_cb;
  s_kernel_cb = NULL;
  cb(s_kernel_cb_data);
}

//! Fire the hold timer and run the posted KernelMain callback.
static void prv_complete_hold(void) {
  cl_assert(stub_new_timer_is_scheduled(HOLD_TIMER_ID));
  stub_new_timer_fire(HOLD_TIMER_ID);
  prv_invoke_kernel_cb();
}

//! A button press and release, which is all auto-lock needs to see to restart.
static void prv_activity(void) {
  prv_press(BUTTON_ID_UP);
  prv_release(BUTTON_ID_UP);
}

//! Let the idle timer expire and run the posted KernelMain callback.
static void prv_expire_auto_lock(void) {
  cl_assert(stub_new_timer_is_scheduled(AUTO_TIMER_ID));
  stub_new_timer_fire(AUTO_TIMER_ID);
  prv_invoke_kernel_cb();
}

//! Hold Select, the gesture the tests bind by default, until the lock toggles.
static void prv_unlock(void) {
  prv_press(BUTTON_ID_SELECT);
  prv_complete_hold();
  prv_release(BUTTON_ID_SELECT);
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
  s_pref_combo = ButtonLockComboOff;
  s_pref_auto_ms = 0;
  s_pref_auto_paused = false;
  s_pref_auto_scope = ButtonLockAutoScopeBoth;
  s_pref_auto_not_charging = true;
  s_is_plugged = false;
  s_workout_ongoing = false;
  prv_unbind_all();
  prv_bind_hold(BUTTON_ID_SELECT);
  s_modal_enabled = false;
  s_modal_properties = ModalPropertyDefault;

  // Unwind state a previous test may have left behind.
  for (ButtonId id = 0; id < NUM_BUTTONS; id++) {
    prv_release(id);
  }
  if (button_lock_is_locked()) {
    prv_unlock();
  }

  s_watchface_running = true;
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

void test_button_lock__engage_locks(void) {
  button_lock_engage();

  cl_assert(button_lock_is_locked());
  cl_assert_equal_i(s_num_short_pulses, 1);
  cl_assert(!s_touch_enabled);
  cl_assert_equal_s(s_last_dialog_text, "Buttons Locked");
  // Popups must outrank notifications/calls, but stay below pairing/alarms.
  cl_assert_equal_i(s_last_dialog_priority, ModalPriorityAlert);
}

void test_button_lock__engage_without_unlock_gesture_is_refused(void) {
  prv_unbind_all();
  button_lock_engage();
  cl_assert(!button_lock_is_locked());

  // A disabled slot still bound to the action does not count either.
  s_ql_hold_app[BUTTON_ID_SELECT] = APP_ID_BUTTON_LOCK_TOGGLE;
  button_lock_engage();
  cl_assert(!button_lock_is_locked());
  cl_assert_equal_i(s_num_short_pulses, 0);
}

void test_button_lock__unlocked_input_passes_through(void) {
  const int num_timer_starts_before = s_num_new_timer_start_calls;

  // Locking is the watchface's job, the bound gesture is left alone here.
  cl_assert(!prv_press(BUTTON_ID_SELECT));
  cl_assert(!prv_release(BUTTON_ID_SELECT));
  cl_assert(!prv_press(BUTTON_ID_BACK));
  cl_assert(!prv_press(BUTTON_ID_DOWN));
  cl_assert(!prv_release(BUTTON_ID_BACK));
  cl_assert(!prv_release(BUTTON_ID_DOWN));
  cl_assert_equal_i(s_num_new_timer_start_calls, num_timer_starts_before);
  cl_assert(!button_lock_is_locked());
}

void test_button_lock__unlock_by_holding_the_bound_button(void) {
  button_lock_engage();

  cl_assert(prv_press(BUTTON_ID_SELECT));
  cl_assert(stub_new_timer_is_scheduled(HOLD_TIMER_ID));
  cl_assert_equal_i(stub_new_timer_timeout(HOLD_TIMER_ID), 2000);
  prv_complete_hold();

  cl_assert(!button_lock_is_locked());
  cl_assert_equal_i(s_num_double_pulses, 1);
  cl_assert(s_touch_enabled);
  cl_assert_equal_s(s_last_dialog_text, "Buttons Unlocked");
  cl_assert_equal_i(s_last_dialog_priority, ModalPriorityAlert);

  // The DOWN was swallowed, so its UP must be too.
  cl_assert(prv_release(BUTTON_ID_SELECT));
}

void test_button_lock__configurable_hold_duration(void) {
  s_pref_hold_ms = 5000;
  button_lock_engage();

  prv_press(BUTTON_ID_SELECT);
  cl_assert_equal_i(stub_new_timer_timeout(HOLD_TIMER_ID), 5000);
  prv_release(BUTTON_ID_SELECT);
}

void test_button_lock__release_before_timeout_aborts(void) {
  button_lock_engage();

  prv_press(BUTTON_ID_SELECT);
  cl_assert(prv_release(BUTTON_ID_SELECT));
  cl_assert(!stub_new_timer_is_scheduled(HOLD_TIMER_ID));
  cl_assert(button_lock_is_locked());
}

void test_button_lock__other_button_cancels_pending(void) {
  button_lock_engage();

  prv_press(BUTTON_ID_SELECT);
  cl_assert(prv_press(BUTTON_ID_UP));
  cl_assert(!stub_new_timer_is_scheduled(HOLD_TIMER_ID));

  prv_release(BUTTON_ID_UP);
  prv_release(BUTTON_ID_SELECT);
  cl_assert(button_lock_is_locked());
}

void test_button_lock__gesture_held_at_engage_does_not_unlock(void) {
  // The watchface engages the lock while the Quick Launch hold is still down.
  cl_assert(!prv_press(BUTTON_ID_SELECT));
  button_lock_engage();
  cl_assert(button_lock_is_locked());
  cl_assert(!stub_new_timer_is_scheduled(HOLD_TIMER_ID));

  // Its DOWN reached the watchface, so its UP must too.
  cl_assert(!prv_release(BUTTON_ID_SELECT));
  cl_assert(button_lock_is_locked());

  // A fresh hold after the release does count.
  prv_press(BUTTON_ID_SELECT);
  cl_assert(stub_new_timer_is_scheduled(HOLD_TIMER_ID));
  prv_release(BUTTON_ID_SELECT);
}

void test_button_lock__each_hold_slot_unlocks(void) {
  for (ButtonId id = 0; id < NUM_BUTTONS; id++) {
    prv_unbind_all();
    prv_bind_hold(id);
    button_lock_engage();
    cl_assert(button_lock_is_locked());

    prv_press(id);
    prv_complete_hold();
    prv_release(id);
    cl_assert(!button_lock_is_locked());
  }
}

void test_button_lock__combo_slot_unlocks(void) {
  prv_unbind_all();
  s_ql_back_up_app = APP_ID_BUTTON_LOCK_TOGGLE;
  s_ql_back_up_enabled = true;
  button_lock_engage();

  // Back alone is not the gesture.
  prv_press(BUTTON_ID_BACK);
  cl_assert(!stub_new_timer_is_scheduled(HOLD_TIMER_ID));
  prv_press(BUTTON_ID_UP);
  prv_complete_hold();
  cl_assert(!button_lock_is_locked());
  cl_assert(prv_release(BUTTON_ID_BACK));
  cl_assert(prv_release(BUTTON_ID_UP));

  prv_unbind_all();
  s_ql_up_down_app = APP_ID_BUTTON_LOCK_TOGGLE;
  s_ql_up_down_enabled = true;
  button_lock_engage();
  prv_press(BUTTON_ID_UP);
  prv_press(BUTTON_ID_DOWN);
  prv_complete_hold();
  prv_release(BUTTON_ID_UP);
  prv_release(BUTTON_ID_DOWN);
  cl_assert(!button_lock_is_locked());
}

void test_button_lock__changing_gesture_restarts_the_hold(void) {
  prv_unbind_all();
  prv_bind_hold(BUTTON_ID_UP);
  s_ql_up_down_app = APP_ID_BUTTON_LOCK_TOGGLE;
  s_ql_up_down_enabled = true;
  button_lock_engage();

  // The Up hold's timer fires, but Down joins before KernelMain runs the
  // posted callback: the outdated toggle must be dropped.
  prv_press(BUTTON_ID_UP);
  stub_new_timer_fire(HOLD_TIMER_ID);
  prv_press(BUTTON_ID_DOWN);
  cl_assert(stub_new_timer_is_scheduled(HOLD_TIMER_ID));
  prv_invoke_kernel_cb();
  cl_assert(button_lock_is_locked());

  prv_complete_hold();
  cl_assert(!button_lock_is_locked());
  prv_release(BUTTON_ID_UP);
  prv_release(BUTTON_ID_DOWN);
}

void test_button_lock__locked_swallows_input_and_hints(void) {
  button_lock_engage();

  s_num_dialogs_created = 0;
  cl_assert(prv_press(BUTTON_ID_UP));
  cl_assert(prv_release(BUTTON_ID_UP));
  cl_assert_equal_i(s_num_dialogs_created, 1);
  cl_assert_equal_s(s_last_dialog_text, "Hold Center to unlock");

  // Hint popup is not re-created while still on screen.
  cl_assert(prv_press(BUTTON_ID_DOWN));
  cl_assert(prv_release(BUTTON_ID_DOWN));
  cl_assert_equal_i(s_num_dialogs_created, 1);

  // Once it unloaded, another press shows it again.
  s_dialog_callbacks.unload(NULL);
  cl_assert(prv_press(BUTTON_ID_DOWN));
  cl_assert(prv_release(BUTTON_ID_DOWN));
  cl_assert_equal_i(s_num_dialogs_created, 2);
}

void test_button_lock__hint_names_the_first_bound_gesture(void) {
  prv_unbind_all();
  s_ql_up_down_app = APP_ID_BUTTON_LOCK_TOGGLE;
  s_ql_up_down_enabled = true;
  s_ql_back_up_app = APP_ID_BUTTON_LOCK_TOGGLE;
  s_ql_back_up_enabled = true;
  button_lock_engage();

  prv_press(BUTTON_ID_SELECT);
  prv_release(BUTTON_ID_SELECT);
  cl_assert_equal_s(s_last_dialog_text, "Hold Back + Up to unlock");

  // Hold gestures come first.
  prv_bind_hold(BUTTON_ID_BACK);
  s_dialog_callbacks.unload(NULL);
  prv_press(BUTTON_ID_SELECT);
  prv_release(BUTTON_ID_SELECT);
  cl_assert_equal_s(s_last_dialog_text, "Hold Back to unlock");
}

void test_button_lock__hint_rises_above_an_interrupting_modal(void) {
  button_lock_engage();

  // An alarm would render over a hint pushed at ModalPriorityAlert, so the hint
  // goes onto the alarm's own stack instead.
  s_modal_enabled = true;
  s_modal_properties = ModalProperty_Exists;
  s_modal_top_focused_priority = ModalPriorityAlarm;

  cl_assert(prv_press(BUTTON_ID_UP));
  cl_assert(prv_release(BUTTON_ID_UP));
  cl_assert_equal_s(s_last_dialog_text, "Hold Center to unlock");
  cl_assert_equal_i(s_last_dialog_priority, ModalPriorityAlarm);
}

void test_button_lock__hint_stays_at_alert_below_alert_modals(void) {
  button_lock_engage();

  // An incoming call is below Alert, where the hint is already visible.
  s_modal_enabled = true;
  s_modal_properties = ModalProperty_Exists;
  s_modal_top_focused_priority = ModalPriorityPhone;

  cl_assert(prv_press(BUTTON_ID_UP));
  cl_assert(prv_release(BUTTON_ID_UP));
  cl_assert_equal_i(s_last_dialog_priority, ModalPriorityAlert);
}

void test_button_lock__lock_toasts_stay_at_alert(void) {
  s_modal_enabled = true;
  s_modal_properties = ModalProperty_Exists;
  s_modal_top_focused_priority = ModalPriorityAlarm;

  // The lock/unlock feedback must never cover an alarm.
  button_lock_engage();
  cl_assert(button_lock_is_locked());
  cl_assert_equal_s(s_last_dialog_text, "Buttons Locked");
  cl_assert_equal_i(s_last_dialog_priority, ModalPriorityAlert);

  prv_unlock();
  cl_assert(!button_lock_is_locked());
  cl_assert_equal_s(s_last_dialog_text, "Buttons Unlocked");
  cl_assert_equal_i(s_last_dialog_priority, ModalPriorityAlert);
}

void test_button_lock__unlock_restores_touch_pref(void) {
  button_lock_engage();
  cl_assert(!s_touch_enabled);

  s_touch_pref_enabled = false;
  prv_unlock();
  cl_assert(!button_lock_is_locked());
  // Touch comes back to the persisted pref, not blindly on.
  cl_assert(!s_touch_enabled);

  button_lock_engage();
  s_touch_pref_enabled = true;
  prv_unlock();
  cl_assert(s_touch_enabled);
}

void test_button_lock__continuous_hold_toggles_once(void) {
  button_lock_engage();

  prv_press(BUTTON_ID_SELECT);
  prv_complete_hold();
  cl_assert(!button_lock_is_locked());

  // Still holding: the timer must not be re-armed.
  cl_assert(!stub_new_timer_is_scheduled(HOLD_TIMER_ID));

  prv_release(BUTTON_ID_SELECT);
  cl_assert(!button_lock_is_locked());
}

void test_button_lock__timer_fire_after_release_race(void) {
  button_lock_engage();
  prv_press(BUTTON_ID_SELECT);

  // Timer fires, but the gesture is released before KernelMain runs the
  // posted callback: the toggle must not happen.
  stub_new_timer_fire(HOLD_TIMER_ID);
  prv_release(BUTTON_ID_SELECT);
  prv_invoke_kernel_cb();

  cl_assert(button_lock_is_locked());
}

void test_button_lock__unlock_while_hint_visible_pops_it(void) {
  button_lock_engage();

  s_num_dialogs_created = 0;
  prv_press(BUTTON_ID_UP);
  prv_release(BUTTON_ID_UP);
  cl_assert_equal_i(s_num_dialogs_created, 1);

  s_num_dialogs_popped = 0;
  prv_unlock();
  cl_assert(!button_lock_is_locked());
  cl_assert_equal_i(s_num_dialogs_popped, 1);
  cl_assert_equal_s(s_last_dialog_text, "Buttons Unlocked");
}

void test_button_lock__rebinding_the_last_gesture_releases_the_lock(void) {
  prv_bind_hold(BUTTON_ID_BACK);
  button_lock_engage();

  // Another gesture still releases the lock, so it stays.
  s_ql_hold_enabled[BUTTON_ID_SELECT] = false;
  button_lock_handle_prefs_changed();
  prv_invoke_kernel_cb();
  cl_assert(button_lock_is_locked());

  s_ql_hold_app[BUTTON_ID_BACK] = APP_ID_MUSIC;
  button_lock_handle_prefs_changed();
  prv_invoke_kernel_cb();
  cl_assert(!button_lock_is_locked());
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

void test_button_lock__auto_lock_needs_an_unlock_gesture(void) {
  prv_unbind_all();
  s_pref_auto_ms = 30000;
  prv_activity();
  cl_assert(!stub_new_timer_is_scheduled(AUTO_TIMER_ID));
}

void test_button_lock__rebinding_the_last_gesture_stops_the_countdown(void) {
  s_pref_auto_ms = 30000;
  prv_activity();
  cl_assert(stub_new_timer_is_scheduled(AUTO_TIMER_ID));

  prv_unbind_all();
  button_lock_handle_prefs_changed();
  prv_invoke_kernel_cb();
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
  button_lock_engage();
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

  prv_unlock();
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
