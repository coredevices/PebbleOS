/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "button_lock.h"

#include "applib/ui/dialogs/dialog.h"
#include "applib/ui/dialogs/dialog_private.h"
#include "applib/ui/dialogs/simple_dialog.h"
#include "applib/ui/vibes.h"
#include "kernel/event_loop.h"
#include "kernel/ui/modals/modal_manager.h"
#include "process_management/app_manager.h"
#include "pbl/services/battery/battery_state.h"
#include "shell/normal/watchface.h"
#include "shell/prefs.h"
#include "system/passert.h"
#include "pbl/services/new_timer/new_timer.h"
#include "pbl/services/i18n/i18n.h"
#include <pbl/logging/logging.h>

#ifdef CONFIG_TOUCH
#include "pbl/services/touch/touch.h"
#endif

#ifdef CONFIG_SERVICE_ACTIVITY
#include "pbl/services/activity/workout_service.h"
#endif

#define BUTTON_LOCK_COMBO            ((1 << BUTTON_ID_BACK) | (1 << BUTTON_ID_DOWN))
#define BUTTON_LOCK_POPUP_TIMEOUT_MS (1800)

static TimerID s_combo_timer = TIMER_INVALID_ID;
static TimerID s_auto_timer = TIMER_INVALID_ID;
//! Auto-lock stays disarmed until the first activity after boot, so a fresh
//! boot never locks itself before the user has touched the watch.
static bool s_auto_armed;
static uint8_t s_buttons_held;
//! Deliver a button UP iff its DOWN was delivered, so click recognizers in
//! the app/watchface never see an unbalanced press.
static uint8_t s_downs_delivered;
static bool s_combo_pending;
//! Set once a hold toggled the lock; blocks re-triggering until all buttons
//! are released, so a continuous hold toggles exactly once.
static bool s_combo_consumed;
static bool s_toggle_cancelled;
static bool s_locked;
static SimpleDialog *s_hint_dialog;

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
  if (s_hint_dialog) {
    return;
  }
  s_hint_dialog = prv_push_popup(i18n_noop("Hold Back + Down to unlock"), &s_hint_dialog_callbacks,
                                 prv_hint_priority());
}

static void prv_pop_hint_popup(void) {
  if (!s_hint_dialog) {
    return;
  }
  dialog_pop(simple_dialog_get_dialog(s_hint_dialog));
  s_hint_dialog = NULL;
}

static void prv_auto_lock_update(void);
static void prv_auto_lock_timer_cb(void *data);

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

//! KernelMain callback posted by the combo hold timer.
static void prv_toggle_lock_cb(void *data) {
  if (s_toggle_cancelled || !s_combo_pending) {
    return;
  }
  s_combo_consumed = true;
  prv_set_locked(!s_locked);
}

//! Runs on the NewTimer thread; just bounce to KernelMain.
static void prv_combo_timer_cb(void *data) {
  launcher_task_add_callback(prv_toggle_lock_cb, NULL);
}

// Auto-lock
///////////////////////////////////////////////////////////////////////////////

//! Auto-lock needs the combo configured: without it the lock could never be
//! released. The pause is owned by the Quick Launch action.
static bool prv_auto_lock_enabled(void) {
  return (shell_prefs_get_button_lock_hold_ms() != 0) &&
         (shell_prefs_get_button_lock_auto_ms() != 0) &&
         !shell_prefs_get_button_lock_auto_paused();
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

//! (Re)start the idle timer, or stop it when auto-lock cannot engage.
static void prv_auto_lock_update(void) {
  if (s_auto_timer == TIMER_INVALID_ID) {
    return;
  }
  if (s_locked || s_combo_pending || !s_auto_armed || !prv_auto_lock_enabled()) {
    new_timer_stop(s_auto_timer);
    return;
  }
  PBL_ASSERTN(new_timer_start(s_auto_timer, shell_prefs_get_button_lock_auto_ms(),
                              prv_auto_lock_timer_cb, NULL, 0 /* flags */));
}

//! KernelMain callback posted by the idle timer.
static void prv_auto_lock_expired_cb(void *data) {
  if (s_locked || s_combo_pending || !s_auto_armed || !prv_auto_lock_enabled()) {
    return;
  }
  if (prv_auto_lock_postponed()) {
    prv_auto_lock_update();
    return;
  }
  prv_set_locked(true);
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
  if (s_locked && (shell_prefs_get_button_lock_hold_ms() == 0)) {
    // The unlock combo just went away, so nothing could release the lock.
    prv_set_locked(false);
    return;
  }
  prv_auto_lock_update();
}

void button_lock_handle_prefs_changed(void) {
  launcher_task_add_callback(prv_prefs_changed_cb, NULL);
}

void button_lock_init(void) {
  s_combo_timer = new_timer_create();
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
    s_buttons_held |= (1 << button_id);
  } else {
    s_buttons_held &= ~(1 << button_id);
  }
  if (s_buttons_held == 0) {
    s_combo_consumed = false;
  }

  const bool combo_held = (s_buttons_held == BUTTON_LOCK_COMBO) && !s_combo_consumed &&
                          (shell_prefs_get_button_lock_hold_ms() != 0);

  if (combo_held && !s_combo_pending) {
    s_combo_pending = true;
    s_toggle_cancelled = false;
    launcher_cancel_force_quit();
    if (!s_locked && app_manager_is_watchface_running()) {
      // Kill the first combo button's armed quick launch long click.
      watchface_reset_click_manager();
    }
    PBL_ASSERTN(new_timer_start(s_combo_timer, shell_prefs_get_button_lock_hold_ms(),
                                prv_combo_timer_cb, NULL, 0 /* flags */));
    // Swallow the combo-completing DOWN: the first combo button's DOWN was already delivered
    // (its click fires like with the quick launch combos), the second button must stay
    // invisible so no click recognizer arms for it. Its UP is swallowed via s_downs_delivered.
    return true;
  }
  if (!combo_held && s_combo_pending) {
    s_combo_pending = false;
    s_toggle_cancelled = true;
    new_timer_stop(s_combo_timer);
  }

  if (!s_locked) {
    prv_auto_lock_note_activity();
  }

  if (is_down) {
    if (s_locked || s_combo_pending) {
      if (s_locked && !s_combo_pending) {
        prv_show_hint_popup();
      }
      return true;
    }
    s_downs_delivered |= (1 << button_id);
    return false;
  }

  const bool deliver = (s_downs_delivered & (1 << button_id));
  s_downs_delivered &= ~(1 << button_id);
  return !deliver;
}
