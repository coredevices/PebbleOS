/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sdl_graphics.h"

#include "applib/graphics/graphics.h"
#include "applib/graphics/8_bit/framebuffer.h"
#include "util/circular_cache.h"

#include <stdint.h>
#include <stdio.h>
#include <SDL.h>

static GContext s_gcontext = {};
static FrameBuffer fb = {};
static SDL_Surface *screen = NULL;

bool sdl_graphics_init(void) {
  if (!(screen = SDL_SetVideoMode(DISP_COLS, DISP_ROWS, 8 /* bits/pixel */, SDL_HWSURFACE))) {
    return false;
  }

  framebuffer_init(&fb, &(GSize) {DISP_COLS, DISP_ROWS});
  s_gcontext = (GContext) {
    .dest_bitmap = (GBitmap) {
      .addr = screen->pixels,
      .row_size_bytes = DISP_COLS,
      .info = (BitmapInfo) {.format = GBITMAP_NATIVE_FORMAT },
      .bounds = (GRect) { { 0, 0 }, { DISP_COLS, DISP_ROWS } },
      .data_row_infos = NULL,
    },
    .parent_framebuffer = &fb,
    .parent_framebuffer_vertical_offset = 0,
    .lock = false
  };

  graphics_context_set_default_drawing_state(&s_gcontext, GContextInitializationMode_App);

  return true;
}

GContext *sdl_graphics_get_gcontext(void) {
  return &s_gcontext;
}

void sdl_graphics_render(void) {
  SDL_Flip(screen);
}
