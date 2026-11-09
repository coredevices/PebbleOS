/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "getting_started_button_combo.h"

#include <pbl/kernel/work.h>
#include <pbl/logging/logging.h>
#include <pbl/util/size.h>

#include <applib/graphics/gtypes.h>
#include <applib/ui/app_window_stack.h>
#include <apps/core/spinner_ui_window.h>
#include <kernel/util/factory_reset.h>
#include <mfg/mfg_mode/mfg_factory_mode.h>
#include <process_management/process_manager.h>

static void prv_timeout_expired(struct pbl_work *work);

// Not in the state: that is app memory, which goes away without a deinit if the app is killed.
static PBL_DELAYED_WORK_DEFINE(s_combo_timer, prv_timeout_expired);
static void (*s_combo_callback)(void *data);

void getting_started_button_combo_init(GettingStartedButtonComboState *state,
                                       GettingStartedButtonComboCallback select_callback) {
  *state = (GettingStartedButtonComboState){
    .buttons_held_bitset = 0,
    .select_callback = select_callback
  };
}

void getting_started_button_combo_deinit(GettingStartedButtonComboState *state) {
  pbl_delayed_work_cancel(&s_combo_timer);
}

static void prv_factory_reset(void *not_used) {
  factory_reset(false /* should_shutdown */);
}

static void prv_down_cb(void *data) {
  Window *spinner_window = spinner_ui_window_get(PBL_IF_COLOR_ELSE(GColorBlue, GColorDarkGray));
  app_window_stack_push(spinner_window, false /* animated */);

  // Factory reset on SysWorkq so the animation gets priority
  pbl_sys_work_call(prv_factory_reset, nullptr);
}

#ifdef CONFIG_RECOVERY_FW
static void prv_mfg_mode_cb(void *data) {
  mfg_enter_mfg_mode_and_launch_app();
}
#endif

static void prv_timeout_expired(struct pbl_work *work) {
  PBL_LOG_INFO("Button combo timeout expired!");

  // Timeout expired, jump over the app thread to do the thing.
  process_manager_send_callback_event_to_process(PebbleTask_App, s_combo_callback, nullptr);
}

static void prv_update_state(GettingStartedButtonComboState *state) {
  const uint32_t COMBO_HOLD_MS = 5 * 1000; // Wait for 5 seconds

  // Map of button combos -> callback to call if we hit it.
  const struct {
    uint8_t desired_bitset;
    void (*callback)(void *);
  } BUTTON_COMBOS[] = {
    {(1 << BUTTON_ID_SELECT), state->select_callback},
    {(1 << BUTTON_ID_DOWN), prv_down_cb},
#ifdef CONFIG_RECOVERY_FW
    {(1 << BUTTON_ID_UP) | (1 << BUTTON_ID_SELECT), prv_mfg_mode_cb},
#endif
  };

  for (unsigned int i = 0; i < ARRAY_LENGTH(BUTTON_COMBOS); ++i) {
    if (state->buttons_held_bitset == BUTTON_COMBOS[i].desired_bitset) {
      PBL_LOG_DBG("Starting timer for combo #%d", i);

      // Canceled first, so an expiry cannot pick up the new combo's callback.
      pbl_delayed_work_cancel(&s_combo_timer);
      s_combo_callback = BUTTON_COMBOS[i].callback;
      pbl_delayed_work_reschedule(&pbl_sys_workq_high, &s_combo_timer, PBL_MSEC(COMBO_HOLD_MS));
      return;
    }
  }

  PBL_LOG_DBG("Stopping combo timer");

  // No combo found, cancel the timer. It's harmless to call this if the timer isn't running.
  pbl_delayed_work_cancel(&s_combo_timer);
}

void getting_started_button_combo_button_pressed(GettingStartedButtonComboState *state,
                                                 ButtonId button_id) {
  pbl_bitset8_set(&state->buttons_held_bitset, button_id);
  prv_update_state(state);
}

void getting_started_button_combo_button_released(GettingStartedButtonComboState *state,
                                                  ButtonId button_id) {
  pbl_bitset8_clear(&state->buttons_held_bitset, button_id);
  prv_update_state(state);
}
