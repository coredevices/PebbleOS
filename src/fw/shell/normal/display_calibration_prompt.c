/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#if PLATFORM_SPALDING

#include "display_calibration_prompt.h"

#include "applib/ui/dialogs/confirmation_dialog.h"
#include "apps/system_apps/settings/settings_display_calibration.h"
#include "kernel/event_loop.h"
#include "kernel/ui/modals/modal_manager.h"
#include "mfg/mfg_info.h"
#include "mfg/mfg_serials.h"
#include "resource/resource_ids.auto.h"
#include "services/common/i18n/i18n.h"
#include "services/common/new_timer/new_timer.h"
#include "shell/prefs.h"
#include "util/size.h"

// The calibration screen will be changing the screen offsets, so it's best that it remains on top
// of most other modals (generic, alerts, etc) to prevent confusion about the screen's alignment.
static const ModalPriority MODAL_PRIORITY = ModalPriorityCritical;

static void prv_calibrate_confirm_pop(ClickRecognizerRef recognizer, void *context) {
  i18n_free_all(context);
  confirmation_dialog_pop((ConfirmationDialog *)context);
}

static void prv_calibrate_confirm_cb(ClickRecognizerRef recognizer, void *context) {
  settings_display_calibration_push(modal_manager_get_window_stack(MODAL_PRIORITY));
  prv_calibrate_confirm_pop(recognizer, context);
}

static void prv_calibrate_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, prv_calibrate_confirm_cb);
  window_single_click_subscribe(BUTTON_ID_DOWN, prv_calibrate_confirm_pop);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_calibrate_confirm_pop);
}

static TimerID s_timer = TIMER_INVALID_ID;

static void prv_push_calibration_dialog(void *data) {
  shell_prefs_set_should_prompt_display_calibration(false);

  ConfirmationDialog *confirmation_dialog = confirmation_dialog_create("Calibrate Prompt");
  Dialog *dialog = confirmation_dialog_get_dialog(confirmation_dialog);

  dialog_set_text(dialog, i18n_get("Your screen may need calibration. Calibrate it now?",
                                   confirmation_dialog));
  dialog_set_background_color(dialog, GColorMediumAquamarine);
  dialog_set_icon(dialog, RESOURCE_ID_GENERIC_PIN_TINY);
  confirmation_dialog_set_click_config_provider(confirmation_dialog,
                                                prv_calibrate_click_config);
  confirmation_dialog_push(confirmation_dialog,
                           modal_manager_get_window_stack(MODAL_PRIORITY));
}

static bool prv_display_has_user_offset(void) {
  GPoint display_offset = shell_prefs_get_display_offset();
  GPoint mfg_display_offset = mfg_info_get_disp_offsets();
  return (!gpoint_equal(&display_offset, &mfg_display_offset));
}

static void prv_timer_callback(void *data) {
  new_timer_delete(s_timer);
  s_timer = TIMER_INVALID_ID;

  // last check: make sure we need to display the prompt in case something changed in the
  // time that the timer was waiting.
  if (!shell_prefs_should_prompt_display_calibration()) {
    return;
  }

  launcher_task_add_callback(prv_push_calibration_dialog, NULL);
}

T_STATIC bool prv_is_known_misaligned_serial_number(const char *serial) {
  // Filter watches known to be misaligned based on the serial number. This is possible because
  // Serial numbers are represented as strings as described in:
  // https://pebbletechnology.atlassian.net/wiki/display/DEV/Hardware+Serial+Numbering
  // All watches of the same model produced from the same manufacturer on the same date, on the
  // same manufacturing line, will share the same first 8 characters of the serial number. In this
  // way, batches which are misaligned can be identified by a string comparison on these characters.
  //
  // NOTE: This also conveniently excludes test automation boards, so the dialog should not
  // appear during integration tests.
  const char *ranges[] = { "Q402445E" };
  for (size_t i = 0; i < ARRAY_LENGTH(ranges); i++) {
    if (strncmp(serial, ranges[i], strlen(ranges[i])) == 0) {
      return true;
    }
  }
  return false;
}

static bool prv_is_potentially_misaligned_watch() {
  return !prv_display_has_user_offset() &&
         prv_is_known_misaligned_serial_number(mfg_get_serial_number());
}

void display_calibration_prompt_show_if_needed(void) {
  if (!prv_is_potentially_misaligned_watch()) {
    shell_prefs_set_should_prompt_display_calibration(false);
    return;
  }

  if (shell_prefs_should_prompt_display_calibration()) {
    s_timer = new_timer_create();
    const uint32_t prompt_delay_time_ms = MS_PER_SECOND * SECONDS_PER_MINUTE;
    new_timer_start(s_timer, prompt_delay_time_ms, prv_timer_callback, NULL, 0 /* flags */);
  }
}

#endif
