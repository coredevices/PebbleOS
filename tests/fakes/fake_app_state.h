/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_graphics_context.h"

#include "applib/ui/content_indicator.h"
#include "kernel/ui/kernel_ui.h"
#include "stubs_app_state.h"

void fake_app_state_init(void) {
  // Setup graphics context
  fake_graphics_context_init();
  s_app_state_framebuffer = fake_graphics_context_get_framebuffer();

  // Setup content indicator
  ContentIndicatorsBuffer *buffer = content_indicator_get_current_buffer();
  content_indicator_init_buffer(buffer);
}
