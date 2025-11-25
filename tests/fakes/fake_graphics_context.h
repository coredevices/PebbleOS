/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/graphics.h"
#include "applib/graphics/framebuffer.h"

#if !FAKE_GRAPHICS_CONTEXT_C
#include "fw/graphics/util.h"
#include "stubs_app_state.h"
#endif

GContext *fake_graphics_context_get_context(void);

FrameBuffer *fake_graphics_context_get_framebuffer(void);

void fake_graphics_context_init(void);

#define FAKE_GRAPHICS_CONTEXT_CHECK_DEST_BITMAP(name) ({ \
  cl_check(gbitmap_pbi_eq(&fake_graphics_context_get_context()->dest_bitmap, name)); \
})

#define FAKE_GRAPHICS_CONTEXT_CHECK_DEST_BITMAP_FILE() ({ \
  cl_check(gbitmap_pbi_eq(&fake_graphics_context_get_context()->dest_bitmap, TEST_PBI_FILE)); \
})
