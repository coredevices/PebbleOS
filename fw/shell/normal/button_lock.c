/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "button_lock.h"

#include <pbl/logging/logging.h>
#include <pbl/services/battery/battery_state.h>
#include <pbl/services/i18n/i18n.h>
#include <pbl/services/new_timer/new_timer.h>
#include <pbl/util/size.h>

#include <applib/ui/dialogs/dialog.h>
#include <applib/ui/dialogs/dialog_private.h>
#include <applib/ui/dialogs/simple_dialog.h>
#include <applib/ui/vibes.h>
#include <kernel/event_loop.h>
#include <kernel/ui/modals/modal_manager.h>
#include <resource/resource_ids.auto.h>
#include <shell/normal/quick_launch.h>
#include <shell/prefs.h>
#include <shell/system_app_ids.auto.h>
#include <system/passert.h>

#ifdef CONFIG_TOUCH
#include <pbl/services/touch/touch.h>
#endif

#ifdef CONFIG_SERVICE_ACTIVITY
#include <pbl/services/activity/workout_service.h>
#endif

#define BUTTON_MASK(id)              (1 << (id))
#define BUTTON_LOCK_POPUP_TIMEOUT_MS (1800)

typedef struct {
  //! The buttons held for the gesture.
  uint8_t mask;
  //! The unlock hint naming the gesture.
  const char *hint;
} ButtonLockGesture;

//! The Quick Launch gestures that can carry the lock action, in the order the
//! hint picks from. Tap gestures are left out on purpose.
static const ButtonLockGesture s_quick_launch_gestures[] = {
  {BUTTON_MASK(BUTTON_ID_UP), i18n_noop("Hold Up to unlock")},
  {BUTTON_MASK(BUTTON_ID_SELECT), i18n_noop("Hold Center to unlock")},
  {BUTTON_MASK(BUTTON_ID_DOWN), i18n_noop("Hold Down to unlock")},
  {BUTTON_MASK(BUTTON_ID_BACK), i18n_noop("Hold Back to unlock")},
  {BUTTON_MASK(BUTTON_ID_BACK) | BUTTON_MASK(BUTTON_ID_UP), i18n_noop("Hold Back + Up to unlock")},
  {BUTTON_MASK(BUTTON_ID_UP) | BUTTON_MASK(BUTTON_ID_DOWN), i18n_noop("Hold Up + Down to unlock")},
};

static TimerID s_hold_timer = TIMER_INVALID_ID;
static TimerID s_auto_timer = TIMER_INVALID_ID;
//! Auto-lock stays disarmed until the first activity after boot, so a fresh
//! boot never locks itself before the user has touched the watch.
static bool s_auto_armed;
static uint8_t s_buttons_held;
//! Deliver a button UP iff its DOWN was delivered, so click recognizers in
//! the app/watchface never see an unbalanced press.
static uint8_t s_downs_delivered;
//! The gesture currently being held toward a toggle, 0 if none.
static uint8_t s_pending_gesture;
//! Bumped on every hold start, so a toggle posted by an outdated hold is dropped.
static uint32_t s_hold_generation;
//! Set once a hold toggled the lock, or the lock engaged with buttons held;
//! blocks any gesture until all buttons are released.
static bool s_gesture_consumed;
static bool s_locked;
static SimpleDialog *s_hint_dialog;

// Gestures
///////////////////////////////////////////////////////////////////////////////

static bool prv_quick_launch_gesture_is_lock(uint8_t mask) {
  switch (mask) {
    case BUTTON_MASK(BUTTON_ID_BACK) | BUTTON_MASK(BUTTON_ID_UP):
      return quick_launch_combo_back_up_is_enabled() &&
             (quick_launch_combo_back_up_get_app() == APP_ID_BUTTON_LOCK_TOGGLE);
    case BUTTON_MASK(BUTTON_ID_UP) | BUTTON_MASK(BUTTON_ID_DOWN):
      return quick_launch_combo_up_down_is_enabled() &&
             (quick_launch_combo_up_down_get_app() == APP_ID_BUTTON_LOCK_TOGGLE);
    default:
      for (ButtonId button = 0; button < NUM_BUTTONS; button++) {
        if (mask == BUTTON_MASK(button)) {
          return quick_launch_is_enabled(button) &&
                 (quick_launch_get_app(button) == APP_ID_BUTTON_LOCK_TOGGLE);
        }
      }
      return false;
  }
}

//! The first configured unlock gesture, or NULL if there is none.
static const ButtonLockGesture *prv_first_unlock_gesture(void) {
  for (size_t i = 0; i < ARRAY_LENGTH(s_quick_launch_gestures); i++) {
    if (prv_quick_launch_gesture_is_lock(s_quick_launch_gestures[i].mask)) {
      return &s_quick_launch_gestures[i];
    }
  }
  return NULL;
}

bool button_lock_has_unlock_gesture(void) {
  return prv_first_unlock_gesture() != NULL;
}

//! The gesture the held buttons form, or 0 if they form none.
static uint8_t prv_held_gesture(void) {
  if (s_gesture_consumed || (s_buttons_held == 0)) {
    return 0;
  }
  if (s_locked && prv_quick_launch_gesture_is_lock(s_buttons_held)) {
    return s_buttons_held;
  }
  return 0;
}

// Popups
///////////////////////////////////////////////////////////////////////////////

static void prv_hint_dialog_unload(void *context) {
  s_hint_dialog = NULL;
}

static const DialogCallbacks s_hint_dialog_callbacks = {
  .unload = prv_hint_dialog_unload,
};

static SimpleDialog *prv_push_popup(const char *text, const DialogCallbacks *callbacks,
                                    ModalPriority priority) {
  SimpleDialog *simple_dialog = simple_dialog_create("ButtonLock");
  Dialog *dialog = simple_dialog_get_dialog(simple_dialog);
  const char *msg = i18n_get(text, dialog);
  dialog_set_text(dialog, msg);
  dialog_set_icon(dialog, RESOURCE_ID_BUTTON_LOCK);
  dialog_set_timeout(dialog, BUTTON_LOCK_POPUP_TIMEOUT_MS);
  if (callbacks) {
    dialog_set_callbacks(dialog, callbacks, NULL);
  }
  i18n_free(msg, dialog);
  simple_dialog_push(simple_dialog, modal_manager_get_window_stack(priority));
  return simple_dialog;
}

//! The hint answers a button press, so it has to be readable no matter what is
//! on screen: at Alert an alarm or a pairing prompt would render over it and
//! the watch would just look dead. Pushing onto the interrupting modal's own
//! stack puts the hint on top of it for the popup's lifetime, without adding a
//! priority above ModalPriorityAlarm.
static ModalPriority prv_hint_priority(void) {
  const ModalPriority top = modal_manager_get_top_focused_priority();
  return (top > ModalPriorityAlert) ? top : ModalPriorityAlert;
}

static void prv_show_hint_popup(void) {
  const ButtonLockGesture *gesture = prv_first_unlock_gesture();
  if (s_hint_dialog || !gesture) {
    return;
  }
  s_hint_dialog = prv_push_popup(gesture->hint, &s_hint_dialog_callbacks, prv_hint_priority());
}

static void prv_pop_hint_popup(void) {
  if (!s_hint_dialog) {
    return;
  }
  dialog_pop(simple_dialog_get_dialog(s_hint_dialog));
  s_hint_dialog = NULL;
}

// Lock state
///////////////////////////////////////////////////////////////////////////////

static void prv_auto_lock_update(void);

//! Engage or release the lock and give the user the matching feedback.
//! Must run on KernelMain.
static void prv_set_locked(bool locked) {
  s_locked = locked;
  PBL_LOG_DBG("Button lock %s", s_locked ? "engaged" : "released");

#ifdef CONFIG_TOUCH
  if (s_locked) {
    touch_service_set_globally_enabled(false);
  } else {
    // touch_is_globally_enabled() is the persisted user pref, not the runtime
    // switch flipped above, so this restores the user's touch setting.
    touch_service_set_globally_enabled(touch_is_globally_enabled());
  }
#endif

  if (s_locked) {
    vibes_short_pulse();
  } else {
    vibes_double_pulse();
  }

  prv_pop_hint_popup();
  // Alert: above notifications/calls so the feedback is visible over them, but
  // below BT pairing and alarms, which must never be hidden by a lock toast the
  // user did not ask for.
  prv_push_popup(s_locked ? i18n_noop("Buttons Locked") : i18n_noop("Buttons Unlocked"), NULL,
                 ModalPriorityAlert);

  prv_auto_lock_update();
}

static void prv_stop_hold(void) {
  s_pending_gesture = 0;
  new_timer_stop(s_hold_timer);
}

//! KernelMain callback posted by the hold timer.
static void prv_hold_complete_cb(void *data) {
  if (((uintptr_t)data != s_hold_generation) || !s_pending_gesture) {
    return;
  }
  s_pending_gesture = 0;
  s_gesture_consumed = true;
  prv_set_locked(!s_locked);
}

//! Runs on the NewTimer thread; just bounce to KernelMain.
static void prv_hold_timer_cb(void *data) {
  launcher_task_add_callback(prv_hold_complete_cb, data);
}

void button_lock_engage(void) {
  if (s_locked || !button_lock_has_unlock_gesture()) {
    return;
  }
  prv_stop_hold();
  // The gesture that triggered the lock is usually still held; it must not
  // count toward unlocking until it was released.
  s_gesture_consumed = (s_buttons_held != 0);
  prv_set_locked(true);
}

// Auto-lock
///////////////////////////////////////////////////////////////////////////////

static void prv_auto_lock_timer_cb(void *data);

//! Auto-lock needs an unlock gesture, without it the lock could never be
//! released. The pause is owned by the Auto-Lock Quick Launch action.
static bool prv_auto_lock_enabled(void) {
  return (shell_prefs_get_button_lock_auto_ms() != 0) &&
         !shell_prefs_get_button_lock_auto_paused() && button_lock_has_unlock_gesture();
}

static bool prv_auto_lock_scope_applies(void) {
  bool workout_ongoing = false;
#ifdef CONFIG_SERVICE_ACTIVITY
  workout_ongoing = workout_service_is_workout_ongoing();
#endif
  switch (shell_prefs_get_button_lock_auto_scope()) {
    case ButtonLockAutoScopeGeneralUse:
      return !workout_ongoing;
    case ButtonLockAutoScopeDuringActivity:
      return workout_ongoing;
    default:
      return true;
  }
}

//! Conditions that postpone auto-locking rather than disable it. They can all
//! change without any user activity, so the timer is re-armed instead of
//! dropped and the state is re-checked when it expires.
static bool prv_auto_lock_postponed(void) {
  if (shell_prefs_get_button_lock_auto_not_charging() && battery_get_charge_state().is_plugged) {
    return true;
  }
  if (!prv_auto_lock_scope_applies()) {
    return true;
  }
  // Never lock the user out of a focused modal, e.g. an alarm or an incoming call.
  return modal_manager_get_enabled() && !(modal_manager_get_properties() & ModalProperty_Unfocused);
}

static bool prv_auto_lock_can_engage(void) {
  return !s_locked && !s_pending_gesture && s_auto_armed && prv_auto_lock_enabled();
}

//! (Re)start the idle timer, or stop it when auto-lock cannot engage.
static void prv_auto_lock_update(void) {
  if (s_auto_timer == TIMER_INVALID_ID) {
    return;
  }
  if (!prv_auto_lock_can_engage()) {
    new_timer_stop(s_auto_timer);
    return;
  }
  PBL_ASSERTN(new_timer_start(s_auto_timer, shell_prefs_get_button_lock_auto_ms(),
                              prv_auto_lock_timer_cb, NULL, 0 /* flags */));
}

//! KernelMain callback posted by the idle timer.
static void prv_auto_lock_expired_cb(void *data) {
  if (!prv_auto_lock_can_engage()) {
    return;
  }
  if (prv_auto_lock_postponed()) {
    prv_auto_lock_update();
    return;
  }
  button_lock_engage();
}

//! Runs on the NewTimer thread; just bounce to KernelMain.
static void prv_auto_lock_timer_cb(void *data) {
  launcher_task_add_callback(prv_auto_lock_expired_cb, NULL);
}

//! Note the user is around and restart the idle countdown.
static void prv_auto_lock_note_activity(void) {
  s_auto_armed = true;
  prv_auto_lock_update();
}

void button_lock_handle_activity(void) {
  if (s_locked) {
    return;
  }
  prv_auto_lock_note_activity();
}

void button_lock_handle_charger_change(bool is_plugged) {
  prv_auto_lock_update();
}

static void prv_prefs_changed_cb(void *data) {
  if (s_locked && !button_lock_has_unlock_gesture()) {
    // The unlock gesture just went away, so nothing could release the lock.
    prv_stop_hold();
    prv_set_locked(false);
    return;
  }
  prv_auto_lock_update();
}

void button_lock_handle_prefs_changed(void) {
  launcher_task_add_callback(prv_prefs_changed_cb, NULL);
}

void button_lock_init(void) {
  s_hold_timer = new_timer_create();
  s_auto_timer = new_timer_create();
}

#if UNITTEST
//! Put auto-lock back into its just-booted state, which is otherwise only
//! reachable by rebooting.
void button_lock_disarm_auto_lock_for_test(void) {
  s_auto_armed = false;
  new_timer_stop(s_auto_timer);
}
#endif

bool button_lock_is_locked(void) {
  return s_locked;
}

bool button_lock_handle_button_event(PebbleEvent *e) {
  const ButtonId button_id = e->button.button_id;
  const bool is_down = (e->type == PEBBLE_BUTTON_DOWN_EVENT);

  if (is_down) {
    s_buttons_held |= BUTTON_MASK(button_id);
  } else {
    s_buttons_held &= ~BUTTON_MASK(button_id);
  }
  if (s_buttons_held == 0) {
    s_gesture_consumed = false;
  }

  // A press or release that changes the gesture restarts the hold, e.g. from a
  // held Up to an Up + Down combo.
  const uint8_t gesture = prv_held_gesture();
  if (gesture != s_pending_gesture) {
    prv_stop_hold();
    if (gesture) {
      s_pending_gesture = gesture;
      s_hold_generation++;
      PBL_ASSERTN(new_timer_start(s_hold_timer, shell_prefs_get_button_lock_hold_ms(),
                                  prv_hold_timer_cb, (void *)(uintptr_t)s_hold_generation,
                                  0 /* flags */));
    }
  }

  if (!s_locked) {
    prv_auto_lock_note_activity();
  }

  if (is_down) {
    if (s_locked) {
      if (!s_pending_gesture) {
        prv_show_hint_popup();
      }
      return true;
    }
    s_downs_delivered |= BUTTON_MASK(button_id);
    return false;
  }

  const bool deliver = (s_downs_delivered & BUTTON_MASK(button_id));
  s_downs_delivered &= ~BUTTON_MASK(button_id);
  return !deliver;
}
