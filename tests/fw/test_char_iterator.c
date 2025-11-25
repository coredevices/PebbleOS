/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/iterator.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/utf8.h"
#include "applib/graphics/text_layout_private.h"

#include "clar.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

///////////////////////////////////////////////////////////
// Stubs
#include "stubs_logging.h"
#include "stubs_passert.h"

#include "stubs_app_state.h"
#include "stubs_fonts.h"
#include "stubs_graphics_context.h"
#include "stubs_gbitmap.h"
#include "stubs_heap.h"
#include "stubs_text_resources.h"
#include "stubs_text_render.h"
#include "stubs_pbl_malloc.h"
#include "stubs_reboot_reason.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"
#include "stubs_compiled_with_legacy2_sdk.h"

///////////////////////////////////////////////////////////
// Fakes

static GContext s_ctx;
static FrameBuffer s_fb;

size_t framebuffer_get_size_bytes(FrameBuffer *f) {
  return FRAMEBUFFER_SIZE_BYTES;
}

///////////////////////////////////////////////////////////
// Tests

void test_char_iterator__initialize(void) {
  framebuffer_init(&s_fb, &(GSize) {DISP_COLS, DISP_ROWS});
  graphics_context_init(&s_ctx, &s_fb, GContextInitializationMode_App);
}

void test_char_iterator__test_string_empty(void) {
  bool success = false;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, "");
  cl_assert(success);

  const TextBoxParams text_box_params = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
  };

  Iterator char_iter;
  CharIterState char_iter_state;
  char_iter_init(&char_iter, &char_iter_state, &text_box_params, utf8_bounds.start);
  Utf8IterState* utf8_iter_state = (Utf8IterState*) &char_iter_state.utf8_iter_state;

  cl_assert(!iter_next(&char_iter));
  cl_assert(!iter_next(&char_iter));
  cl_assert(!iter_next(&char_iter));
}

void test_char_iterator__decode_test_string_length_one(void) {
  Iterator char_iter;
  CharIterState char_iter_state;
  Utf8IterState* utf8_iter_state = (Utf8IterState*) &char_iter_state.utf8_iter_state;

  // Single-byte/ASCII
  bool success = false;
  const Utf8Bounds utf8_bounds_single_byte = utf8_get_bounds(&success, "A");
  cl_assert(success);

  const TextBoxParams text_box_params_single_byte = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds_single_byte,
  };

  char_iter_init(&char_iter, &char_iter_state, &text_box_params_single_byte, utf8_bounds_single_byte.start);

  cl_assert(!iter_next(&char_iter));
  cl_assert(!iter_next(&char_iter));
  cl_assert(!iter_next(&char_iter));

  // Multi-byte char
  success = false;
  const Utf8Bounds utf8_bounds_multi_byte = utf8_get_bounds(&success, "\xc3\xb0");
  cl_assert(success);

  const TextBoxParams text_box_params_multi_byte = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds_multi_byte,
  };

  char_iter_init(&char_iter, &char_iter_state, &text_box_params_multi_byte, utf8_bounds_multi_byte.start);

  cl_assert(!iter_next(&char_iter));
  cl_assert(!iter_next(&char_iter));
  cl_assert(!iter_next(&char_iter));
}

void test_char_iterator__decode_test_string_with_formatting_char(void) {
  Iterator char_iter;
  CharIterState char_iter_state;
  Utf8IterState* utf8_iter_state = (Utf8IterState*) &char_iter_state.utf8_iter_state;

  // Skip over codepoints that aren't newline and < 0x20
  bool success = false;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, "A\nB\x01\x02");
  cl_assert(success);

  const TextBoxParams text_box_params = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
  };

  char_iter_init(&char_iter, &char_iter_state, &text_box_params, utf8_bounds.start);

  cl_assert(utf8_iter_state->codepoint == 'A');
  cl_assert(iter_next(&char_iter));
  cl_assert(utf8_iter_state->codepoint == '\n');
  cl_assert(iter_next(&char_iter));
  cl_assert(utf8_iter_state->codepoint == 'B');
  cl_assert(!iter_next(&char_iter));
  cl_assert(!iter_next(&char_iter));
  cl_assert(!iter_next(&char_iter));
}

void test_char_iterator__decode_test_string_with_initial_formatting_char(void) {
  Iterator char_iter;
  CharIterState char_iter_state;
  Utf8IterState* utf8_iter_state = (Utf8IterState*) &char_iter_state.utf8_iter_state;

  // Skip over codepoints that aren't newline and < 0x20
  bool success = false;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, "\x02\x11\x41\nB\x01 \x02");
  cl_assert(success);

  const TextBoxParams text_box_params = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
  };

  char_iter_init(&char_iter, &char_iter_state, &text_box_params, utf8_bounds.start);

  cl_assert(utf8_iter_state->codepoint == 0x02);
  cl_assert(iter_next(&char_iter));
  cl_assert(utf8_iter_state->codepoint == 'A'); // 0x41
  cl_assert(iter_next(&char_iter));
  cl_assert(utf8_iter_state->codepoint == '\n');
  cl_assert(iter_next(&char_iter));
  cl_assert(utf8_iter_state->codepoint == 'B');
  cl_assert(iter_next(&char_iter));
  cl_assert(utf8_iter_state->codepoint == ' ');
  cl_assert(!iter_next(&char_iter));
  cl_assert(!iter_next(&char_iter));
  cl_assert(!iter_next(&char_iter));
}

