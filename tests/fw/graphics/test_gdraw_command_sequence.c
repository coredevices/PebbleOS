/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/graphics/gdraw_command.h"
#include "applib/graphics/gdraw_command_list.h"
#include "applib/graphics/gdraw_command_frame.h"
#include "applib/graphics/gdraw_command_private.h"
#include "applib/graphics/gdraw_command_sequence.h"

#include "applib/graphics/gtypes.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/gpath.h"

#include "util/size.h"

#include "stubs_applib_resource.h"
#include "stubs_memory_layout.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"

// Stubs
void graphics_context_set_stroke_color(GContext* ctx, GColor color) {}
void graphics_context_set_fill_color(GContext* ctx, GColor color) {}
void graphics_context_set_stroke_width(GContext* ctx, uint8_t stroke_width) {}
void gpath_draw_stroke(GContext* ctx, GPath* path, bool open) {}
void gpath_draw_filled(GContext* ctx, GPath *path) {}
void graphics_draw_circle(GContext* ctx, GPoint p, uint16_t radius) {}
void graphics_fill_circle(GContext* ctx, GPoint p, uint16_t radius) {}
void graphics_context_move_draw_box(GContext* ctx, GPoint offset) {}
void graphics_line_draw_precise_stroked(GContext* ctx, GPointPrecise p0, GPointPrecise p1) {}
void gpath_fill_precise_internal(GContext *ctx, GPointPrecise *points, size_t num_points) {}
void gpath_draw_outline_precise_internal(GContext *ctx, GPointPrecise *points, size_t num_points,
                                         bool open) {}
typedef uint16_t ResourceId;
const uint8_t *resource_get_builtin_bytes(ResAppNum app_num, uint32_t resource_id,
					  uint32_t *num_bytes_out) { return NULL; }


// setup and teardown
void test_gdraw_command_sequence__initialize(void) {

}

void test_gdraw_command_sequence__cleanup(void) {

}

size_t prv_create_test_sequence(GDrawCommandSequence **sequence_ptr) {
  size_t size = sizeof(GDrawCommandSequence) + (sizeof(GDrawCommandFrame) * 2) +
      (sizeof(GDrawCommand) * 4) + (sizeof(GPoint) * 9);

  GDrawCommandSequence *sequence = calloc(1, size);
  *sequence_ptr = sequence;

  *sequence = (GDrawCommandSequence){
    .version = GDRAW_COMMAND_VERSION,
    .num_frames = 2,
    .play_count = 1,
  };

  GDrawCommandFrame *frame;
  frame = &sequence->frames[0];
  *frame = (GDrawCommandFrame) {
    .duration = 15,
  };
  frame->command_list = (GDrawCommandList) {
    .num_commands = 3
  };
  GDrawCommand *command;
  command = gdraw_command_list_get_command(&frame->command_list, 0);
  GPoint points1[] = { { 3, 97 }, {5, 5} };
  *command = (GDrawCommand) {
    .type = GDrawCommandTypePath,
    .hidden = false,
    .stroke_color = GColorRed,
    .stroke_width = 1,
    .fill_color = GColorBlue,
    .path_open = false,
    .num_points = ARRAY_LENGTH(points1),
  };
  memcpy(command->points, points1, sizeof(points1));

  command = gdraw_command_list_get_command(&frame->command_list, 1);
  *command = (GDrawCommand) {
    .type = GDrawCommandTypeCircle,
    .hidden = false,
    .stroke_color = GColorGreen,
    .stroke_width = 1,
    .fill_color = GColorOrange,
    .radius = 300,
    .num_points = 1,
  };
  command->points[0] = (GPoint) { 1, 2 };

  command = gdraw_command_list_get_command(&frame->command_list, 2);
  GPoint points2[] = { { 6, 7 }, {5, 5}, { 0, 0 } };
  *command = (GDrawCommand) {
    .type = GDrawCommandTypePath,
    .hidden = false,
    .stroke_color = GColorGreen,
    .stroke_width = 1,
    .fill_color = GColorPurple,
    .path_open = false,
    .num_points = ARRAY_LENGTH(points2),
  };
  memcpy(command->points, points2, sizeof(points2));

  frame = (GDrawCommandFrame *)(command->points + command->num_points);
  *frame = (GDrawCommandFrame) {
    .duration = 30,
  };
  frame->command_list = (GDrawCommandList) {
    .num_commands = 1
  };
  command = gdraw_command_list_get_command(&frame->command_list, 0);
  points2[0].x++; // increment x value to distinguish draw command from command in previous frame
  *command = (GDrawCommand) {
    .type = GDrawCommandTypePath,
    .hidden = false,
    .stroke_color = GColorRed,
    .stroke_width = 5,
    .fill_color = GColorBlack,
    .path_open = false,
    .num_points = ARRAY_LENGTH(points2),
  };
  memcpy(command->points, points2, sizeof(points2));

  return size;
}

// tests
void test_gdraw_command_sequence__validate(void) {
  GDrawCommandSequence *sequence;
  size_t size = prv_create_test_sequence(&sequence);

  cl_assert_equal_i(size, gdraw_command_sequence_get_data_size(sequence));
  cl_assert(gdraw_command_sequence_validate(sequence, size));
  cl_assert(!gdraw_command_sequence_validate(sequence, size - 1));
  cl_assert(!gdraw_command_sequence_validate(sequence, size + 1));
  cl_assert(!gdraw_command_sequence_validate(sequence, 0));

  sequence->num_frames = 0;
  cl_assert(!gdraw_command_sequence_validate(sequence, size));
  sequence->num_frames = 1;
  cl_assert(!gdraw_command_sequence_validate(sequence, size));
  sequence->num_frames = 3;
  cl_assert(!gdraw_command_sequence_validate(sequence, size));
  sequence->num_frames = 2;

  sequence->version = 0xFF;
  cl_assert(!gdraw_command_sequence_validate(sequence, size));

  free(sequence);
}

void test_gdraw_command_sequence__get_frame_by_elapsed(void) {
  GDrawCommandSequence *sequence;
  prv_create_test_sequence(&sequence);

  GDrawCommandFrame *frame = gdraw_command_sequence_get_frame_by_elapsed(sequence, 0);
  cl_assert_equal_p(frame, gdraw_command_sequence_get_frame_by_index(sequence, 0));
  cl_assert_equal_p(frame, gdraw_command_sequence_get_frame_by_elapsed(sequence, 14));

  frame = gdraw_command_sequence_get_frame_by_elapsed(sequence, 15);
  cl_assert_equal_p(frame, gdraw_command_sequence_get_frame_by_index(sequence, 1));;
  cl_assert_equal_p(frame, gdraw_command_sequence_get_frame_by_elapsed(sequence, 44));
  cl_assert_equal_p(frame, gdraw_command_sequence_get_frame_by_elapsed(sequence, 45));
  cl_assert_equal_p(frame, gdraw_command_sequence_get_frame_by_elapsed(sequence, 46));

  // test that frame is skipped when the duration is zero (first frame shown will be the first one
  // with non-zero duration
  frame = gdraw_command_sequence_get_frame_by_elapsed(sequence, 0);
  frame->duration = 0;
  frame = gdraw_command_sequence_get_frame_by_elapsed(sequence, 0);
  cl_assert_equal_p(frame, gdraw_command_sequence_get_frame_by_index(sequence, 1));
  frame = gdraw_command_sequence_get_frame_by_index(sequence, 0);
  frame->duration = 15;

  // test that the sequence loops when the play count is greater than 1
  sequence->play_count = 2;
  frame = gdraw_command_sequence_get_frame_by_elapsed(sequence, 45);
  cl_assert_equal_p(frame, gdraw_command_sequence_get_frame_by_index(sequence, 0));
  frame = gdraw_command_sequence_get_frame_by_elapsed(sequence, 45 + 15);
  cl_assert_equal_p(frame, gdraw_command_sequence_get_frame_by_index(sequence, 1));

  // test that the sequence loops infinitely when the play count is infinite
  sequence->play_count = (uint16_t)PLAY_COUNT_INFINITE;
  frame = gdraw_command_sequence_get_frame_by_elapsed(sequence, 45 * 5);
  cl_assert_equal_p(frame, gdraw_command_sequence_get_frame_by_index(sequence, 0));
  frame = gdraw_command_sequence_get_frame_by_elapsed(sequence, (45 + 15) * 5);
  cl_assert_equal_p(frame, gdraw_command_sequence_get_frame_by_index(sequence, 1));

  // test that the sequence returns the last frame if the play count is zero
  sequence->play_count = 0;
  frame = gdraw_command_sequence_get_frame_by_elapsed(sequence, 1);
  cl_assert_equal_p(frame, gdraw_command_sequence_get_frame_by_index(sequence, 1));

  free(sequence);
}

void test_gdraw_command_sequence__get_frame_by_index(void) {
  GDrawCommandSequence *sequence;
  prv_create_test_sequence(&sequence);

  GDrawCommandFrame *frame = gdraw_command_sequence_get_frame_by_index(sequence, 0);
  cl_assert_equal_i(frame->duration, 15);
  cl_assert_equal_i(frame->command_list.num_commands, 3);
  GDrawCommand *command = gdraw_command_list_get_command(&frame->command_list, 2);
  cl_assert_equal_i(command->type, GDrawCommandTypePath);
  cl_assert_equal_i(command->num_points, 3);
  cl_assert_equal_i(command->stroke_color.argb, GColorGreenARGB8);
  cl_assert_equal_i(command->fill_color.argb, GColorPurpleARGB8);
  cl_assert_equal_i(command->points[0].x, 6);

  frame = gdraw_command_sequence_get_frame_by_index(sequence, 1);
  cl_assert_equal_i(frame->duration, 30);
  cl_assert_equal_i(frame->command_list.num_commands, 1);
  command = gdraw_command_list_get_command(&frame->command_list, 0);
  cl_assert_equal_i(command->type, GDrawCommandTypePath);
  cl_assert_equal_i(command->num_points, 3);
  cl_assert_equal_i(command->stroke_color.argb, GColorRedARGB8);
  cl_assert_equal_i(command->fill_color.argb, GColorBlackARGB8);
  cl_assert_equal_i(command->points[0].x, 7);

  cl_assert_equal_p(gdraw_command_sequence_get_frame_by_index(sequence, 2), NULL);

  free(sequence);
}

void test_gdraw_command_sequence__clone(void) {
  cl_assert_equal_p(gdraw_command_sequence_clone(NULL), NULL);

  GDrawCommandSequence *sequence;
  prv_create_test_sequence(&sequence);

  GDrawCommandSequence *clone = gdraw_command_sequence_clone(sequence);
  cl_assert(clone != sequence);
  size_t expected_size = gdraw_command_sequence_get_data_size(sequence);
  cl_assert_equal_i(gdraw_command_sequence_get_data_size(clone), expected_size);
  cl_assert_equal_i(0, memcmp(clone, sequence, expected_size));

  free(sequence);
}
