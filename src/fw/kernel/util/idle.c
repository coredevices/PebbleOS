/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdbool.h>

static bool s_idle_allowed = true;

void idle_set_enabled(bool enable) {
  s_idle_allowed = enable;
}

bool idle_is_allowed(void) {
  return s_idle_allowed;
}

void command_scheduler_force_active(void) {
  idle_set_enabled(false);
}

void command_scheduler_resume_normal(void) {
  idle_set_enabled(true);
}
