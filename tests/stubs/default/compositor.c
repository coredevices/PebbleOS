/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
#include <pbl/services/compositor/compositor.h>

PBL_WEAK void compositor_transition(const CompositorTransition *type) {
}

PBL_WEAK GRect compositor_get_scaled_app_frame(GSize app_size) {
  return GRect(0, 0, PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT);
}
