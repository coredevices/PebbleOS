/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "process_state/app_state/app_state.h"

GContext* app_get_current_graphics_context(void) {
  return app_state_get_graphics_context();
}

