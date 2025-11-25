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
#include <string.h>

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

void test_word_iterator__initialize(void) {
  framebuffer_init(&s_fb, &(GSize) {DISP_COLS, DISP_ROWS});
  graphics_context_init(&s_ctx, &s_fb, GContextInitializationMode_App);
}

void test_word_iterator__test_string_empty(void) {
  // Allocate mutable types
  Iterator word_iter = (Iterator) { 0 };
  WordIterState word_iter_state = (WordIterState) { 0 };

  // Allocate immutable types
  bool success = false;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, "");
  cl_assert(success);

  const TextBoxParams text_box_params = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
  };

  // Init mutable types
  word_iter_init(&word_iter, &word_iter_state, &s_ctx, &text_box_params, utf8_bounds.start);

  // Tests
  cl_assert(word_iter_state.current.start == word_iter_state.current.end);
  cl_assert(!iter_next(&word_iter));
  cl_assert(word_iter_state.current.width_px == 0);
}

void test_word_iterator__test_unprintable(void) {
  // Allocate mutable types
  Iterator word_iter = (Iterator) { 0 };
  WordIterState word_iter_state = (WordIterState) { 0 };

  // Allocate immutable types
  bool success = false;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, (char[]) { 0x10, 0x0 });
  cl_assert(success);

  const TextBoxParams text_box_params = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
  };

  // Init mutable types
  word_iter_init(&word_iter, &word_iter_state, &s_ctx, &text_box_params, utf8_bounds.start);

  // Tests
  cl_assert(word_iter_state.current.start == word_iter_state.current.end);
  cl_assert(!iter_next(&word_iter));
  cl_assert(word_iter_state.current.width_px == 0);
}



void test_word_iterator__test_string_single_word(void) {
  // Allocate mutable types
  Iterator word_iter = (Iterator) { 0 };
  WordIterState word_iter_state = (WordIterState) { 0 };

  // Allocate immutable types
  bool success = false;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, "Animal\x02style");
  cl_assert(success);

  const TextBoxParams text_box_params = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
  };

  // Init mutable types
  word_iter_init(&word_iter, &word_iter_state, &s_ctx, &text_box_params, utf8_bounds.start);

  // Tests
  cl_assert(*word_iter_state.current.start == 'A');
  cl_assert(*word_iter_state.current.end == '\0');
  cl_assert(HORIZ_ADVANCE_PX * 11 == word_iter_state.current.width_px);
}

void test_word_iterator__test_string_consecutive_newlines(void) {
  // Allocate mutable types
  Iterator word_iter = (Iterator) { 0 };
  WordIterState word_iter_state = (WordIterState) { 0 };

  // Allocate immutable types
  bool success = false;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, "In\n\n\nN\nout");
  cl_assert(success);

  const TextBoxParams text_box_params = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
  };

  // Init mutable types
  word_iter_init(&word_iter, &word_iter_state, &s_ctx, &text_box_params, utf8_bounds.start);

  // Tests
  cl_assert(*word_iter_state.current.start == 'I');
  cl_assert(*word_iter_state.current.end == '\n');
  cl_assert(HORIZ_ADVANCE_PX * 2 == word_iter_state.current.width_px);

  cl_assert(iter_next(&word_iter));
  cl_assert(*word_iter_state.current.start == '\n');
  cl_assert(*word_iter_state.current.end == '\n');
  cl_assert(0 == word_iter_state.current.width_px);

  cl_assert(iter_next(&word_iter));
  cl_assert(*word_iter_state.current.start == '\n');
  cl_assert(*word_iter_state.current.end == '\n');

  cl_assert(iter_next(&word_iter));
  cl_assert(*word_iter_state.current.start == '\n');
  cl_assert(*word_iter_state.current.end == 'N');

  cl_assert(iter_next(&word_iter));
  cl_assert(*word_iter_state.current.start == 'N');
  cl_assert(*word_iter_state.current.end == '\n');

  cl_assert(iter_next(&word_iter));
  cl_assert(*word_iter_state.current.start == '\n');
  cl_assert(*word_iter_state.current.end == 'o');

  cl_assert(iter_next(&word_iter));
  cl_assert(*word_iter_state.current.start == 'o');
  cl_assert(*word_iter_state.current.end == '\0');

  cl_assert(!iter_next(&word_iter));
}

void test_word_iterator__test_string_terminating_newlines(void) {
  // Allocate mutable types
  Iterator word_iter = (Iterator) { 0 };
  WordIterState word_iter_state = (WordIterState) { 0 };

  // Allocate immutable types
  bool success = false;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, "\nIn\nout\n");
  cl_assert(success);

  const TextBoxParams text_box_params = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
  };

  // Init mutable types
  word_iter_init(&word_iter, &word_iter_state, &s_ctx, &text_box_params, utf8_bounds.start);

  // Tests
  cl_assert(*word_iter_state.current.start == '\n');
  cl_assert(*word_iter_state.current.end == 'I');
  cl_assert(word_iter_state.current.width_px == 0);

  cl_assert(iter_next(&word_iter));
  cl_assert(*word_iter_state.current.start == 'I');
  cl_assert(*word_iter_state.current.end == '\n');
  cl_assert(HORIZ_ADVANCE_PX * 2 == word_iter_state.current.width_px);

  cl_assert(iter_next(&word_iter));
  cl_assert(*word_iter_state.current.start == '\n');
  cl_assert(*word_iter_state.current.end == 'o');

  cl_assert(iter_next(&word_iter));
  cl_assert(*word_iter_state.current.start == 'o');
  cl_assert(*word_iter_state.current.end == '\n');

  cl_assert(iter_next(&word_iter));
  cl_assert(*word_iter_state.current.start == '\n');
  cl_assert(*word_iter_state.current.end == '\0');

  cl_assert(!iter_next(&word_iter));
}

