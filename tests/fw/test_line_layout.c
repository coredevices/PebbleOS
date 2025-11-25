/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util/iterator.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/utf8.h"
#include "applib/graphics/text_layout_private.h"
#include "utf8_test_data.h"

#include "clar.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////////////////////
// Stubs

#include "stubs_logging.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"

#include "stubs_app_state.h"
#include "stubs_fonts.h"
#include "stubs_graphics_context.h"
#include "stubs_gbitmap.h"
#include "stubs_heap.h"
#include "stubs_text_resources.h"
#include "stubs_text_render.h"
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

void test_line_layout__initialize(void) {
  framebuffer_init(&s_fb, &(GSize) {DISP_COLS, DISP_ROWS});
  graphics_context_init(&s_ctx, &s_fb, GContextInitializationMode_App);
}

void line_reset(Line* line, utf8_t* start) {
  line->start = start;
  line->origin = GPoint(0, 0);
  line->height_px = 0;
  line->width_px = 0;
  line->suffix_codepoint = 0;
}

void test_line_layout__test_line_add_word_no_overflow(void) {
  // Allocate mutable types
  Iterator word_iter = ITERATOR_EMPTY;
  WordIterState word_iter_state = WORD_ITER_STATE_EMPTY;
  Line line = { 0 };

  // Allocate immutable types
  bool success = false;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, "Foo bar");
  cl_assert(success);

  const TextBoxParams text_box_params = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
    .box = (GRect) { GPointZero, (GSize) { 7 * HORIZ_ADVANCE_PX + 1, 11 } }
  };
  line.max_width_px = text_box_params.box.size.w;
  line.height_px = text_box_params.box.size.h;

  // Init mutable types
  word_iter_init(&word_iter, &word_iter_state, &s_ctx, &text_box_params, utf8_bounds.start);

  // Tests
  cl_assert(line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 3 * HORIZ_ADVANCE_PX);

  cl_assert(iter_next(&word_iter));

  cl_assert(line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 7 * HORIZ_ADVANCE_PX);

  // Should not have room for another word
  cl_assert(!line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
}

void test_line_layout__test_line_add_word_exact_bounds(void) {
  // Allocate mutable types
  Iterator word_iter = ITERATOR_EMPTY;
  WordIterState word_iter_state = WORD_ITER_STATE_EMPTY;
  Line line = { 0 };
  
  // Allocate immutable types
  bool success = false;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, "Foo bar");
  cl_assert(success);

  const TextBoxParams text_box_params = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
    .box = (GRect) { GPointZero, (GSize) { 7 * HORIZ_ADVANCE_PX, 10 } }
  };
  line.max_width_px = text_box_params.box.size.w;
  line.height_px = text_box_params.box.size.h;

  // Init mutable types
  word_iter_init(&word_iter, &word_iter_state, &s_ctx, &text_box_params, utf8_bounds.start);

  // Tests
  cl_assert(line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 3 * HORIZ_ADVANCE_PX);

  cl_assert(iter_next(&word_iter));

  cl_assert(line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 7 * HORIZ_ADVANCE_PX);

  cl_assert(!line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
}

void test_line_layout__test_line_add_word_horizontal_overflow(void) {
  // Allocate mutable types
  Iterator word_iter = (Iterator) { 0 };
  WordIterState word_iter_state = (WordIterState) { 0 };
  Line line = (Line) { 0 };

  // Allocate immutable types
  bool success = false;

  const char *sentence = "Foo bar";
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, sentence);
  cl_assert(success);

  const TextBoxParams text_box_params = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
      // Width for first word only:
    .box = (GRect) { (GPoint) { 0, 0 }, (GSize) { 3 * HORIZ_ADVANCE_PX, 10 } }
  };
  line.max_width_px = text_box_params.box.size.w;
  line.height_px = text_box_params.box.size.h;

  // Init mutable types
  word_iter_init(&word_iter, &word_iter_state, &s_ctx, &text_box_params, utf8_bounds.start);

  // Tests
  cl_assert(line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 3 * HORIZ_ADVANCE_PX);

  cl_assert(iter_next(&word_iter));

  cl_assert(!line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 3 * HORIZ_ADVANCE_PX);

  cl_assert(!line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(!line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
}

void test_line_layout__test_line_add_word_ideographs(void) {
  // Allocate mutable types
  Iterator word_iter = (Iterator) { 0 };
  WordIterState word_iter_state = (WordIterState) { 0 };
  Line line = (Line) { 0 };

  // Allocate immutable types
  bool success = false;

  const char *sentence = NIHAO_JOINED NIHAO NIHAOMA_JOINED NIHAO_JOINED NIHAO_JOINED;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, sentence);
  cl_assert(success);

  const TextBoxParams text_box_params = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
      // Width for first word only:
    .box = (GRect) { (GPoint) { 0, 0 }, (GSize) { 3 * HORIZ_ADVANCE_PX, 10 } }
  };
  line.max_width_px = text_box_params.box.size.w;
  line.height_px = text_box_params.box.size.h;

  // Init mutable types
  word_iter_init(&word_iter, &word_iter_state, &s_ctx, &text_box_params, utf8_bounds.start);

  // Tests
  cl_assert(line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 2 * HORIZ_ADVANCE_PX);

  cl_assert(iter_next(&word_iter));

  cl_assert(line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 3 * HORIZ_ADVANCE_PX);

  cl_assert(iter_next(&word_iter));

  cl_assert(!line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 3 * HORIZ_ADVANCE_PX);

  // reset line
  line = (Line) { 0 };
  line.max_width_px = text_box_params.box.size.w;
  line.height_px = text_box_params.box.size.h;

  cl_assert(line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 1 * HORIZ_ADVANCE_PX);

  cl_assert(iter_next(&word_iter));

  cl_assert(!line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 1 * HORIZ_ADVANCE_PX);

  // reset line
  line = (Line) { 0 };
  line.max_width_px = text_box_params.box.size.w;
  line.height_px = text_box_params.box.size.h;

  cl_assert(line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 3 * HORIZ_ADVANCE_PX);

  cl_assert(iter_next(&word_iter));

  cl_assert(!line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 3 * HORIZ_ADVANCE_PX);

  // reset line
  line = (Line) { 0 };
  line.max_width_px = text_box_params.box.size.w;
  line.height_px = text_box_params.box.size.h;

  cl_assert(line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 2 * HORIZ_ADVANCE_PX);

  cl_assert(iter_next(&word_iter));

  cl_assert(!line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == 2 * HORIZ_ADVANCE_PX);

  cl_assert(!line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));
  cl_assert(!line_add_word(&s_ctx, &line, &word_iter_state.current, &text_box_params));

}

//! Multi-line, multi-word, multi-hyphenation
void test_line_layout__test_line_add_words_multi_line(void) {
  // Allocate mutable types
  Iterator word_iter = ITERATOR_EMPTY;
  WordIterState word_iter_state = WORD_ITER_STATE_EMPTY;
  Line line = { 0 };
  
  // Allocate immutable types
  bool success = false;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, "Foo b\n\n\nar \nanimalstyle");
  cl_assert(success);

  const TextBoxParams text_box_params = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
    .box = (GRect) { GPointZero, (GSize) { 4 * HORIZ_ADVANCE_PX, 90 } }
  };
  line.max_width_px = text_box_params.box.size.w;
  line.height_px = text_box_params.box.size.h;

  // Init mutable types
  word_iter_init(&word_iter, &word_iter_state, &s_ctx, &text_box_params, utf8_bounds.start);

  // Tests
  // Foo
  cl_assert(*word_iter_state.current.start == 'F');
  cl_assert(*word_iter_state.current.end == ' ');
  cl_assert(word_iter_state.current.width_px == HORIZ_ADVANCE_PX * 3);

  line_reset(&line, utf8_bounds.start);
  cl_assert(line_add_words(&line, &word_iter, NULL));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == HORIZ_ADVANCE_PX * 3);
  cl_assert(line.origin.x == 0);
  cl_assert(line.origin.y == 0);
  cl_assert(line.suffix_codepoint == 0);
  cl_assert(*line.start == 'F');

  // b
  cl_assert(*word_iter_state.current.start == 'b');
  cl_assert(*word_iter_state.current.end == '\n');
  cl_assert(word_iter_state.current.width_px == HORIZ_ADVANCE_PX * 1);

  line_reset(&line, utf8_bounds.start);
  cl_assert(line_add_words(&line, &word_iter, NULL));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == HORIZ_ADVANCE_PX * 1);
  cl_assert(line.origin.x == 0);
  cl_assert(line.suffix_codepoint == 0);
  cl_assert(*line.start == 'b');

  // \n
  cl_assert(*word_iter_state.current.start == '\n');
  cl_assert(*word_iter_state.current.end == '\n');
  cl_assert(word_iter_state.current.width_px == HORIZ_ADVANCE_PX * 0);

  line_reset(&line, utf8_bounds.start);
  cl_assert(line_add_words(&line, &word_iter, NULL));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == HORIZ_ADVANCE_PX * 0);
  cl_assert(line.origin.x == 0);
  cl_assert(line.suffix_codepoint == 0);
  cl_assert(*line.start == '\n');

  // \n
  cl_assert(*word_iter_state.current.start == '\n');
  cl_assert(*word_iter_state.current.end == 'a');
  cl_assert(word_iter_state.current.width_px == HORIZ_ADVANCE_PX * 0);

  line_reset(&line, utf8_bounds.start);
  cl_assert(line_add_words(&line, &word_iter, NULL));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == HORIZ_ADVANCE_PX * 0);
  cl_assert(line.origin.x == 0);
  cl_assert(line.suffix_codepoint == 0);
  cl_assert(*line.start == '\n');

  // ar
  cl_assert(*word_iter_state.current.start == 'a');
  cl_assert(*word_iter_state.current.end == ' ');
  cl_assert(word_iter_state.current.width_px == HORIZ_ADVANCE_PX * 2);

  line_reset(&line, utf8_bounds.start);
  cl_assert(line_add_words(&line, &word_iter, NULL));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == HORIZ_ADVANCE_PX * 3);
  cl_assert(line.origin.x == 0);
  cl_assert(line.suffix_codepoint == 0);
  cl_assert(*line.start == 'a');

  // ani-
  cl_assert(*word_iter_state.current.start == 'a');
  cl_assert(*word_iter_state.current.end == '\0');
  cl_assert(word_iter_state.current.width_px == HORIZ_ADVANCE_PX * 11);

  line_reset(&line, utf8_bounds.start);
  cl_assert(line_add_words(&line, &word_iter, NULL));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == HORIZ_ADVANCE_PX * 4);
  cl_assert(line.origin.x == 0);
  cl_assert(line.suffix_codepoint == '-');
  cl_assert(*line.start == 'a');

  // mal-
  cl_assert(*word_iter_state.current.start == 'm');
  cl_assert(*word_iter_state.current.end == '\0');
  cl_assert(word_iter_state.current.width_px == HORIZ_ADVANCE_PX * 8);

  line_reset(&line, utf8_bounds.start);
  cl_assert(line_add_words(&line, &word_iter, NULL));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == HORIZ_ADVANCE_PX * 4);
  cl_assert(line.origin.x == 0);
  cl_assert(line.suffix_codepoint == '-');
  cl_assert(*line.start == 'm');

  // sty-
  cl_assert(*word_iter_state.current.start == 's');
  cl_assert(*word_iter_state.current.end == '\0');
  cl_assert(word_iter_state.current.width_px == HORIZ_ADVANCE_PX * 5);

  line_reset(&line, utf8_bounds.start);
  cl_assert(line_add_words(&line, &word_iter, NULL));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == HORIZ_ADVANCE_PX * 4);
  cl_assert(line.origin.x == 0);
  cl_assert(line.suffix_codepoint == '-');
  cl_assert(*line.start == 's');

  // le
  cl_assert(*word_iter_state.current.start == 'l');
  cl_assert(*word_iter_state.current.end == '\0');
  cl_assert(word_iter_state.current.width_px == HORIZ_ADVANCE_PX * 2);

  line_reset(&line, utf8_bounds.start);
  cl_assert(false == line_add_words(&line, &word_iter, NULL));
  cl_assert(line.height_px == 10);
  cl_assert(line.width_px == HORIZ_ADVANCE_PX * 2);
  cl_assert(line.origin.x == 0);
  cl_assert(line.suffix_codepoint == 0);
  cl_assert(*line.start == 'l');
}

void test_line_layout__test_walk_lines_down(void) {
  // Allocate mutable types
  Iterator line_iter = ITERATOR_EMPTY;
  LineIterState line_iter_state = { 0 };

  // Allocate immutable types
  bool success = false;
  const Utf8Bounds utf8_bounds = utf8_get_bounds(&success, "\n\n\0");
  cl_assert(success);

  s_ctx.text_draw_state.text_box = (TextBoxParams) {
    .utf8_bounds = &utf8_bounds,
    .box = (GRect) { GPointZero, (GSize) { 7 * HORIZ_ADVANCE_PX, 80 } }
  };
  s_ctx.text_draw_state.line = (Line) {
    .max_width_px = s_ctx.text_draw_state.text_box.box.size.w,
    .height_px = s_ctx.text_draw_state.text_box.box.size.h,
    .start = utf8_bounds.start
  };

  // Init mutable types
  line_iter_init(&line_iter, &line_iter_state, &s_ctx);

  // Tests
  int count = 0;
  while (true) {
    bool is_text_remaining = line_add_words(&s_ctx.text_draw_state.line, &line_iter_state.word_iter, NULL);
    count++;
    if (!is_text_remaining) {
      // Exit after 2 lines
      cl_assert(count == 2);
      break;
    }
    if (!iter_next(&line_iter)) {
      // Should not get here
      cl_assert(false);
      break;
    }
  }
}

