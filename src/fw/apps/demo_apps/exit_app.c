/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "exit_app.h"
#include "applib/app_logging.h"

// Verify that applications can actually call stdlib's exit().
static void s_exit_app_main(void) {

  // Make visible in the debugger if we are running privileged.
  uint32_t value;
   __asm volatile("mrs %0, control" : "=r" (value));
  volatile bool is_privileged = !(value & 0x1);
  APP_LOG(LOG_LEVEL_DEBUG, "Exit app is %sprivileged; now exiting", is_privileged ? "" : "not ");
}

const PebbleProcessMd *exit_app_get_app_info(void) {
  static const PebbleProcessMdSystem s_exit_app_info = {
    .common.main_func = s_exit_app_main,
    .common.is_unprivileged = true,
    .name = "Exit Test"
  };
  return (const PebbleProcessMd*) &s_exit_app_info;
}

