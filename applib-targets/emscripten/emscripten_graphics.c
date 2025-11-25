/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>
#include <string.h>

#include "applib/fonts/fonts.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/gtypes.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/gdraw_command_image.h"
#include "applib/graphics/gdraw_command_sequence.h"
#include "applib/graphics/gdraw_command_frame.h"
#include "applib/graphics/gbitmap_sequence.h"
#include "applib/graphics/text.h"
#include "applib/rockyjs/api/rocky_api_graphics.h"
#include "applib/rockyjs/api/rocky_api_util.h"
#include "applib/rockyjs/api/rocky_api_errors.h"

#include "jerry-api.h"
#include "process_state/app_state/app_state.h"

#include <emscripten/emscripten.h>

static GContext s_gcontext = {};
// FIXME: PBL-43469 Support for changing platforms will require a dynamic framebuffer.
static FrameBuffer s_framebuffer = {};
static TextRenderState s_text_render_state = {};
static UnobstructedAreaState s_unobstructed_area_state = {};

// FIXME: Right now, rocky only supports 1 window anyways
static Window *s_top_window;

GContext *emx_graphics_get_gcontext(void) {
  return &s_gcontext;
}

void *emx_graphics_get_pixels(void) {
  return s_gcontext.dest_bitmap.addr;
}

TextRenderState *app_state_get_text_render_state(void) {
  return &s_text_render_state;
}

Layer** app_state_get_layer_tree_stack(void) {
  static Layer *layer_tree_stack[LAYER_TREE_STACK_SIZE];
  return layer_tree_stack;
}

Layer** kernel_applib_get_layer_tree_stack(void) {
  PBL_ASSERT(0, "Not expected to be called when compiling to applib-emscripten...");
  return NULL;
}

// FIXME: Emscripten is cannot deal with two files with the same name
// (even if the path is different) The framebuffer.c files end up not
// getting linked in.  A longer term fix would be to rename the object
// file in WAF
volatile const int FrameBuffer_MaxX = DISP_COLS;
volatile const int FrameBuffer_MaxY = DISP_ROWS;
void framebuffer_mark_dirty_rect(FrameBuffer* f, GRect rect) {
}

size_t framebuffer_get_size_bytes(FrameBuffer *f) {
  return FRAMEBUFFER_SIZE_BYTES;
}

Window *app_window_stack_get_top_window(void) {
  return s_top_window;
}

void app_window_stack_push(Window *window, bool animated) {
  PBL_ASSERT(!s_top_window, "Already have a window");
  s_top_window = window;
}

GContext *graphics_context_get_current_context(void) {
  return &s_gcontext;
}

// TODO: PBL-43467 Support a user-specified unobstructed area
UnobstructedAreaState *app_state_get_unobstructed_area_state(void) {
  return &s_unobstructed_area_state;
}

void unobstructed_area_service_get_area(UnobstructedAreaState *state, GRect *area_out) {
  *area_out = state->area;
}

// FIXME: PBL-43496 This should take width, height, and format to dynamically
// allocate our framebuffer GBitmap and support changing platforms.
void emx_graphics_init(void) {
  framebuffer_init(&s_framebuffer, &(GSize) {DISP_COLS, DISP_ROWS});
  memset(s_framebuffer.buffer, 0xff, FRAMEBUFFER_SIZE_BYTES);
  framebuffer_dirty_all(&s_framebuffer);
  graphics_context_init(&s_gcontext, &s_framebuffer, GContextInitializationMode_App);

  s_unobstructed_area_state = (UnobstructedAreaState) {
    .area = {
      .origin = { .x = 0, .y = 0 },
      .size = { .w = 144, .h = 168 },
    },
  };
}
