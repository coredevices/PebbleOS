/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/framebuffer.h"

#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_app_state.h"

#define GRAPHICS_FIXTURE_PATH "graphics"
#define GRAPHICS_FIXTURE_OUT_PATH ".."

extern GContext *s_app_state_get_graphics_context;

static void test_graphics_context_reset(GContext* ctx, FrameBuffer* framebuffer) {
  graphics_context_init(ctx, framebuffer, GContextInitializationMode_App);
  framebuffer_clear(framebuffer);
}

static void test_graphics_context_init(GContext* ctx, FrameBuffer* framebuffer) {
  test_graphics_context_reset(ctx, framebuffer);
  s_app_state_get_graphics_context = ctx;  // needed by app_state_get_graphics_context
  graphics_context_set_antialiased(ctx, false);
}
