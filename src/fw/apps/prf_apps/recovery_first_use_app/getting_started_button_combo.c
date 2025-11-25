/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "getting_started_button_combo.h"

#include "applib/ui/app_window_stack.h"
#include "applib/graphics/gtypes.h"

#include "apps/core_apps/spinner_ui_window.h"
#include "kernel/util/factory_reset.h"
#include "mfg/mfg_mode/mfg_factory_mode.h"
#include "process_management/process_manager.h"
#include "services/common/system_task.h"
#if CAPABILITY_HAS_ACCESSORY_CONNECTOR
#include "services/prf/accessory/accessory_imaging.h"
#endif
#include "system/logging.h"
#include "util/size.h"

void getting_started_button_combo_init(GettingStartedButtonComboState *state,
                                       GettingStartedButtonComboCallback select_callback) {
  *state = (GettingStartedButtonComboState) {
    .buttons_held_bitset = 0,
    .combo_timer = new_timer_create(),
    .select_callback = select_callback
  };
}

void getting_started_button_combo_deinit(GettingStartedButtonComboState *state) {
  new_timer_delete(state->combo_timer);
}

static void prv_factory_reset(void *not_used) {
  factory_reset(false /* should_shutdown */);
}

static void prv_down_cb(void *data) {
  Window *spinner_window = spinner_ui_window_get(PBL_IF_COLOR_ELSE(GColorBlue, GColorDarkGray));
  app_window_stack_push(spinner_window, false /* animated */);

  // Factory reset on KernelBG so the animation gets priority
  system_task_add_callback(prv_factory_reset, NULL);
}

#ifdef RECOVERY_FW
static void prv_mfg_mode_cb(void *data) {
#if CAPABILITY_HAS_ACCESSORY_CONNECTOR
  accessory_imaging_enable();
#endif
  mfg_enter_mfg_mode_and_launch_app();
}
#endif

static void prv_timeout_expired(void *data) {
  PBL_LOG(LOG_LEVEL_INFO, "Button combo timeout expired!");

  // Timeout expired, jump over the app thread to do the thing.
  void (*real_callback)(void*) = data;
  process_manager_send_callback_event_to_process(PebbleTask_App, real_callback, NULL);
}

static void prv_update_state(GettingStartedButtonComboState *state) {
  const uint32_t COMBO_HOLD_MS = 5 * 1000; // Wait for 5 seconds

  // Map of button combos -> callback to call if we hit it.
  const struct {
    uint8_t desired_bitset;
    void (*callback)(void*);
  } BUTTON_COMBOS[] = {
    { (1 << BUTTON_ID_SELECT), state->select_callback },
    { (1 << BUTTON_ID_DOWN), prv_down_cb },
#ifdef RECOVERY_FW
    { (1 << BUTTON_ID_UP) | (1 << BUTTON_ID_SELECT), prv_mfg_mode_cb },
#endif
  };

  for (unsigned int i = 0; i < ARRAY_LENGTH(BUTTON_COMBOS); ++i) {
    if (state->buttons_held_bitset == BUTTON_COMBOS[i].desired_bitset) {
      PBL_LOG(LOG_LEVEL_DEBUG, "Starting timer for combo #%d", i);

      new_timer_start(state->combo_timer, COMBO_HOLD_MS, prv_timeout_expired,
                      BUTTON_COMBOS[i].callback, 0);
      return;
    }
  }

  PBL_LOG(LOG_LEVEL_DEBUG, "Stopping combo timer");

  // No combo found, cancel the timer. It's harmless to call this if the timer isn't running.
  new_timer_stop(state->combo_timer);
}

void getting_started_button_combo_button_pressed(GettingStartedButtonComboState *state,
                                                 ButtonId button_id) {
    bitset8_set(&state->buttons_held_bitset, button_id);
    prv_update_state(state);
}

void getting_started_button_combo_button_released(GettingStartedButtonComboState *state,
                                                  ButtonId button_id) {
    bitset8_clear(&state->buttons_held_bitset, button_id);
    prv_update_state(state);
}
