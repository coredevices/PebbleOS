/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! The "Lock Buttons" Quick Launch action. The shell recognizes the app id and
//! engages the button lock directly, without launching anything; holding the
//! same Quick Launch gesture again releases it.

#include "button_lock_toggle.h"

#include <pbl/services/i18n/i18n.h>

static void prv_main(void) {
  // Never runs; the shell engages the lock instead of launching the action.
}

const PebbleProcessMd *button_lock_toggle_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common =
        {
          .main_func = &prv_main,
          .uuid = BUTTON_LOCK_TOGGLE_UUID,
          .visibility = ProcessVisibilityQuickLaunch,
        },
    /// The Quick Launch action that locks all buttons until its gesture is held again.
    .name = i18n_noop("Lock Buttons"),
  };
  return &s_app_info.common;
}
