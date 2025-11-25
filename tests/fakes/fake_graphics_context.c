/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#define FAKE_GRAPHICS_CONTEXT_C (1)

#include "fake_graphics_context.h"

#include "applib/graphics/graphics.h"
#include "applib/graphics/framebuffer.h"

extern GContext *s_app_state_get_graphics_context;

static GContext s_ctx;
static FrameBuffer s_fb;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

GContext *fake_graphics_context_get_context(void) {
  return &s_ctx;
}

FrameBuffer *fake_graphics_context_get_framebuffer(void) {
  return &s_fb;
}

void fake_graphics_context_init(void) {
  framebuffer_init(&s_fb, &(GSize) {DISP_COLS, DISP_ROWS});
  framebuffer_clear(&s_fb);
  graphics_context_init(&s_ctx, &s_fb, GContextInitializationMode_App);
  s_app_state_get_graphics_context = &s_ctx;
}
