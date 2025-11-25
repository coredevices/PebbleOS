/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "clar.h"

#include "applib/graphics/gdraw_command.h"
#include "applib/graphics/gdraw_command_list.h"
#include "applib/graphics/gdraw_command_image.h"
#include "applib/graphics/gdraw_command_private.h"

#include "applib/graphics/gtypes.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_line.h"
#include "applib/graphics/gpath.h"

#include <string.h>

#include "stubs_applib_resource.h"
#include "stubs_app_state.h"
#include "stubs_compiled_with_legacy2_sdk.h"
#include "stubs_heap.h"
#include "stubs_memory_layout.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_resources.h"
#include "stubs_syscalls.h"

extern size_t prv_get_list_max_command_size(GDrawCommandList *command_list);

static GColor s_fill_color;
static GColor s_stroke_color;
static uint8_t s_stroke_width;
static uint16_t s_path_num_points;
static GPoint *s_stroke_points = NULL;
static GPoint *s_fill_points = NULL;
static bool s_path_open;
static uint16_t s_radius;

struct PreciseLine {
  GPointPrecise p0;
  GPointPrecise p1;
} *s_precise_lines = NULL;
static int s_num_precise_lines;

static int s_path_stroke_count;
static int s_path_stroke_precise_count;
static int s_path_fill_count;
static int s_path_fill_precise_count;
static int s_circle_stroke_count;
static int s_circle_fill_count;
static GPoint s_offset;

static GPoint *prv_copy_points(GPoint *points, uint16_t num_points, GPoint offset) {
  s_path_num_points = num_points;
  GPoint *copied_points = malloc(num_points * sizeof(GPoint));
  cl_assert(copied_points != NULL);
  for (int i = 0; i < num_points; i++) {
    copied_points[i] = gpoint_add(points[i], offset);
  }
  return copied_points;
}

static bool prv_compare_points(GPoint *a, GPoint *b, uint16_t num_points) {
  return (memcmp(a, b, sizeof(GPoint) * num_points) == 0);
}

// Stubs
void graphics_context_set_stroke_color(GContext* ctx, GColor color) {
  s_stroke_color = color;
}

void graphics_context_set_fill_color(GContext* ctx, GColor color) {
  s_fill_color = color;
}

void graphics_context_set_antialiased(GContext *ctx, bool enable) {

}

void graphics_context_set_stroke_width(GContext* ctx, uint8_t stroke_width) {
  s_stroke_width = stroke_width;
}

void gpath_draw_stroke(GContext* ctx, GPath* path, bool open) {
  s_stroke_points = prv_copy_points(path->points, path->num_points, s_offset);
  s_path_open = open;
  s_path_stroke_count++;
}

void gpath_fill_precise_internal(GContext *ctx, GPointPrecise *points, size_t num_points) {
  s_fill_points = prv_copy_points((GPoint*)points, num_points, s_offset);
  s_path_fill_precise_count++;
}

void gpath_draw_filled(GContext* ctx, GPath *path) {
  s_fill_points = prv_copy_points(path->points, path->num_points, s_offset);
  s_path_fill_count++;
}

void gpath_draw_outline_precise_internal(GContext *ctx, GPointPrecise *points, size_t num_points,
                                         bool open) {
  s_stroke_points = prv_copy_points((GPoint*)points, num_points, s_offset);
  s_path_open = open;
  s_path_stroke_precise_count++;
}

void graphics_draw_circle(GContext* ctx, GPoint p, uint16_t radius) {
  s_stroke_points = prv_copy_points(&p, 1, s_offset);
  s_radius = radius;
  s_circle_stroke_count++;
}

void graphics_fill_circle(GContext* ctx, GPoint p, uint16_t radius) {
  s_fill_points = prv_copy_points(&p, 1, s_offset);
  s_radius = radius;
  s_circle_fill_count++;
}

void graphics_context_move_draw_box(GContext* ctx, GPoint offset) {
  s_offset = offset;
}

void graphics_line_draw_precise_stroked(GContext* ctx, GPointPrecise p0, GPointPrecise p1) {
  s_precise_lines = realloc(s_precise_lines,
      (s_num_precise_lines + 1) * sizeof(*s_precise_lines));

  s_precise_lines[s_num_precise_lines].p0 = p0;
  s_precise_lines[s_num_precise_lines].p1 = p1;
  s_num_precise_lines++;
}


void prv_reset(void) {
  s_fill_color = GColorClear;
  s_stroke_color = GColorClear;
  s_stroke_width = 0;
  s_path_num_points = 0;
  if (s_stroke_points) {
    free(s_stroke_points);
    s_stroke_points = NULL;
  }
  if (s_fill_points) {
    free(s_fill_points);
    s_fill_points = NULL;
  }
  s_path_open = false;
  s_radius = 0;

  s_path_stroke_count = 0;
  s_path_stroke_precise_count = 0;
  s_path_fill_count = 0;
  s_path_fill_precise_count = 0;
  s_circle_stroke_count = 0;
  s_circle_fill_count = 0;
  s_offset = (GPoint){ 0 };

  if (s_precise_lines) {
    free(s_precise_lines);
    s_precise_lines = NULL;
  }
  s_num_precise_lines = 0;
}

// setup and teardown
void test_gdraw_command__initialize(void) {
  prv_reset();
}

void test_gdraw_command__cleanup(void) {
  if (s_stroke_points) {
    free(s_stroke_points);
    s_stroke_points = NULL;
  }
  if (s_fill_points) {
    free(s_fill_points);
    s_fill_points = NULL;
  }
  if (s_precise_lines) {
    free(s_precise_lines);
    s_precise_lines = NULL;
  }
}

// tests
void test_gdraw_command__draw_command_stroke(void) {
  GDrawCommand *command = malloc(sizeof(GDrawCommand) + (sizeof(GPoint) * 2));
   *command = (GDrawCommand){
    .type = GDrawCommandTypePath,
    .hidden = false,
    .stroke_color = GColorRed,
    .stroke_width = 1,
    .fill_color = GColorBlue,
    .path_open = false,
    .num_points = 2,
  };
  GPoint points[] = {{ 3, 97 }, {5, 5} };
  memcpy(command->points, points, sizeof(points));
  gdraw_command_draw(NULL, command);

  cl_assert_equal_i(s_stroke_color.argb, GColorRedARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorBlueARGB8);
  cl_assert_equal_i(s_stroke_width, 1);
  cl_assert_equal_i(s_path_num_points, 2);
  cl_assert_equal_b(s_path_open, false);
  cl_assert_equal_i(s_path_fill_count, 1);
  cl_assert_equal_i(s_path_stroke_count, 1);
  cl_assert(prv_compare_points(points, s_stroke_points, s_path_num_points));
  cl_assert(prv_compare_points(points, s_fill_points, s_path_num_points));

  prv_reset();
  // set stroke width to zero - fill should be drawn, but not outline
  gdraw_command_set_stroke_width(command, 0);
  gdraw_command_draw(NULL, command);
  cl_assert_equal_i(s_stroke_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorBlueARGB8);
  cl_assert_equal_i(s_stroke_width, 0);
  cl_assert_equal_i(s_path_num_points, 2);
  cl_assert_equal_b(s_path_open, false);
  cl_assert_equal_i(s_path_fill_count, 1);
  cl_assert_equal_i(s_path_stroke_count, 0);
  cl_assert(prv_compare_points(points, s_fill_points, s_path_num_points));
  cl_assert_equal_p(s_stroke_points, NULL);

  prv_reset();
  // make fill color transparent (nothing should be drawn because the stroke width is zero and the
  // fill is transparent
  GColor color = gdraw_command_get_fill_color(command);
  color.a = 0;
  gdraw_command_set_fill_color(command, color);
  gdraw_command_draw(NULL, command);
  cl_assert_equal_i(s_stroke_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_stroke_width, 0);
  cl_assert_equal_i(s_path_num_points, 0);
  cl_assert_equal_b(s_path_open, false);
  cl_assert_equal_i(s_path_fill_count, 0);
  cl_assert_equal_i(s_path_stroke_count, 0);
  cl_assert_equal_p(s_stroke_points, NULL);
  cl_assert_equal_p(s_fill_points, NULL);

  prv_reset();
  // set stroke width to non-zero value. stroke should be drawn, but no fill
  gdraw_command_set_stroke_width(command, 2);
  gdraw_command_draw(NULL, command);
  cl_assert_equal_i(s_stroke_color.argb, GColorRedARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_stroke_width, 2);
  cl_assert_equal_i(s_path_num_points, 2);
  cl_assert_equal_b(s_path_open, false);
  cl_assert_equal_i(s_path_fill_count, 0);
  cl_assert_equal_i(s_path_stroke_count, 1);
  cl_assert(prv_compare_points(points, s_stroke_points, s_path_num_points));
  cl_assert_equal_p(s_fill_points, NULL);

  prv_reset();
  // set stroke color to be transparent and restore fill - fill should be drawn, but no outline
  // should be drawn
  gdraw_command_set_fill_color(command, GColorGreen);
  color = gdraw_command_get_stroke_color(command);
  color.a = 0;
  gdraw_command_set_stroke_color(command, color);
  gdraw_command_draw(NULL, command);
  cl_assert_equal_i(s_stroke_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorGreenARGB8);
  cl_assert_equal_i(s_stroke_width, 0);
  cl_assert_equal_i(s_path_num_points, 2);
  cl_assert_equal_b(s_path_open, false);
  cl_assert_equal_i(s_path_fill_count, 1);
  cl_assert_equal_i(s_path_stroke_count, 0);
  cl_assert_equal_p(s_stroke_points, NULL);
  cl_assert(prv_compare_points(points, s_fill_points, s_path_num_points));

  prv_reset();
  // restore stroke color and change both points
  gdraw_command_set_stroke_color(command, GColorPurple);
  GPoint points2[] = {  { 23, 45 }, { 67, 13} };
  gdraw_command_set_point(command, 0, points2[0]);
  gdraw_command_set_point(command, 1, points2[1]);
  gdraw_command_draw(NULL, command);
  cl_assert_equal_i(s_stroke_color.argb, GColorPurpleARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorGreenARGB8);
  cl_assert_equal_i(s_stroke_width, 2);
  cl_assert_equal_i(s_path_num_points, 2);
  cl_assert_equal_b(s_path_open, false);
  cl_assert_equal_i(s_path_fill_count, 1);
  cl_assert_equal_i(s_path_stroke_count, 1);
  cl_assert(prv_compare_points(points2, s_stroke_points, s_path_num_points));
  cl_assert(prv_compare_points(points2, s_fill_points, s_path_num_points));

  prv_reset();
  // set path to be open
  gdraw_command_set_path_open(command, true);
  gdraw_command_draw(NULL, command);
  cl_assert_equal_i(s_stroke_color.argb, GColorPurpleARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorGreenARGB8);
  cl_assert_equal_i(s_stroke_width, 2);
  cl_assert_equal_i(s_path_num_points, 2);
  cl_assert_equal_b(s_path_open, true);
  cl_assert_equal_i(s_path_fill_count, 1);
  cl_assert_equal_i(s_path_stroke_count, 1);
  cl_assert(prv_compare_points(points2, s_stroke_points, s_path_num_points));
  cl_assert(prv_compare_points(points2, s_fill_points, s_path_num_points));

  prv_reset();
  // set command to be hidden - nothing should be drawn
  gdraw_command_set_hidden(command, true);
  cl_assert_equal_i(s_stroke_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_stroke_width, 0);
  cl_assert_equal_i(s_path_num_points, 0);
  cl_assert_equal_b(s_path_open, false);
  cl_assert_equal_i(s_path_fill_count, 0);
  cl_assert_equal_i(s_path_stroke_count, 0);
  cl_assert_equal_p(s_stroke_points, NULL);
  cl_assert_equal_p(s_fill_points, NULL);

  free(command);
}

void test_gdraw_command__draw_precise_path(void) {
  GDrawCommand *command = malloc(sizeof(GDrawCommand) + (sizeof(GPoint) * 3   ));
   *command = (GDrawCommand){
    .type = GDrawCommandTypePrecisePath,
    .hidden = false,
    .stroke_color = GColorRed,
    .stroke_width = 1,
    .fill_color = GColorBlue,
    .path_open = false,
    .num_points = 3,
  };
  GPointPrecise points[] = {
    { .x.raw_value = 8, .y.raw_value = 17 },
    { .x.raw_value = 4, .y.raw_value = 16 },
    { .x.raw_value = 2,.y.raw_value = 7 }
  };
  memcpy(command->precise_points, points, sizeof(points));
  gdraw_command_draw(NULL, command);

  cl_assert_equal_i(1, s_path_fill_precise_count);
  cl_assert(prv_compare_points((GPoint*)points, s_fill_points, s_path_num_points));

  cl_assert_equal_i(1, s_path_stroke_precise_count);
  cl_assert_equal_b(false, s_path_open);
  cl_assert(prv_compare_points((GPoint*)points, s_stroke_points, s_path_num_points));

  prv_reset();
  // change to open path and ensure that only draws 2 lines
  command->path_open = true;
  gdraw_command_draw(NULL, command);
  cl_assert_equal_i(1, s_path_stroke_precise_count);
  cl_assert_equal_b(true, s_path_open);
  cl_assert(prv_compare_points((GPoint*)points, s_stroke_points, s_path_num_points));

  free(command);
}

void test_gdraw_command__draw_circle(void) {
  GDrawCommand *command = malloc(sizeof(GDrawCommand) + sizeof(GPoint));
   *command = (GDrawCommand){
    .type = GDrawCommandTypeCircle,
    .hidden = false,
    .stroke_color = GColorGreen,
    .stroke_width = 1,
    .fill_color = GColorOrange,
    .radius = 300,
    .num_points = 1,
  };
  GPoint center = { 15, 17 };
  command->points[0] = center;
  gdraw_command_draw(NULL, command);

  cl_assert_equal_i(s_stroke_color.argb, GColorGreenARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorOrangeARGB8);
  cl_assert_equal_i(s_stroke_width, 1);
  cl_assert_equal_i(s_path_num_points, 1);
  cl_assert_equal_b(s_radius, 300);
  cl_assert_equal_i(s_circle_fill_count, 1);
  cl_assert_equal_i(s_circle_stroke_count, 1);
  cl_assert(prv_compare_points(&center, s_stroke_points, s_path_num_points));
  cl_assert(prv_compare_points(&center, s_fill_points, s_path_num_points));

  prv_reset();
  // set stroke width to zero - fill should be drawn, but not outline
  gdraw_command_set_stroke_width(command, 0);
  gdraw_command_draw(NULL, command);
  cl_assert_equal_i(s_stroke_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorOrangeARGB8);
  cl_assert_equal_i(s_stroke_width, 0);
  cl_assert_equal_i(s_path_num_points, 1);
  cl_assert_equal_b(s_radius, 300);
  cl_assert_equal_i(s_circle_fill_count, 1);
  cl_assert_equal_i(s_circle_stroke_count, 0);
  cl_assert_equal_p(s_stroke_points, NULL);
  cl_assert(prv_compare_points(&center, s_fill_points, s_path_num_points));

  prv_reset();
  // make fill color transparent (nothing should be drawn because the stroke width is zero and the
  // fill is transparent
  GColor color = gdraw_command_get_fill_color(command);
  color.a = 0;
  gdraw_command_set_fill_color(command, color);
  gdraw_command_draw(NULL, command);
  cl_assert_equal_i(s_stroke_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_stroke_width, 0);
  cl_assert_equal_i(s_path_num_points, 0);
  cl_assert_equal_b(s_radius, 0);
  cl_assert_equal_i(s_circle_fill_count, 0);
  cl_assert_equal_i(s_circle_stroke_count, 0);
  cl_assert_equal_p(s_stroke_points, NULL);
  cl_assert_equal_p(s_fill_points, NULL);

  prv_reset();
  // set stroke width to non-zero value. stroke should be drawn, but no fill
  gdraw_command_set_stroke_width(command, 2);
  gdraw_command_draw(NULL, command);
  cl_assert_equal_i(s_stroke_color.argb, GColorGreenARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_stroke_width, 2);
  cl_assert_equal_i(s_path_num_points, 1);
  cl_assert_equal_b(s_radius, 300);
  cl_assert_equal_i(s_circle_fill_count, 0);
  cl_assert_equal_i(s_circle_stroke_count, 1);
  cl_assert(prv_compare_points(&center, s_stroke_points, s_path_num_points));
  cl_assert_equal_p(s_fill_points, NULL);

  prv_reset();
  // set stroke color to be transparent and restore fill - fill should be drawn, but no outline
  // should be drawn
  gdraw_command_set_fill_color(command, GColorRed);
  color = gdraw_command_get_stroke_color(command);
  color.a = 0;
  gdraw_command_set_stroke_color(command, color);
  gdraw_command_draw(NULL, command);
  cl_assert_equal_i(s_stroke_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorRedARGB8);
  cl_assert_equal_i(s_stroke_width, 0);
  cl_assert_equal_i(s_path_num_points, 1);
  cl_assert_equal_b(s_path_open, false);
  cl_assert_equal_i(s_circle_fill_count, 1);
  cl_assert_equal_i(s_circle_stroke_count, 0);
  cl_assert_equal_p(s_stroke_points, NULL);
  cl_assert(prv_compare_points(&center, s_fill_points, s_path_num_points));

  prv_reset();
  // restore stroke color and set radius to zero - only a stroke should be drawn
  gdraw_command_set_stroke_color(command, GColorPurple);
  gdraw_command_set_radius(command, 0);
  gdraw_command_draw(NULL, command);
  cl_assert_equal_i(s_stroke_color.argb, GColorPurpleARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_stroke_width, 2);
  cl_assert_equal_i(s_path_num_points, 1);
  cl_assert_equal_b(s_radius, 0);
  cl_assert_equal_i(s_circle_fill_count, 0);
  cl_assert_equal_i(s_circle_stroke_count, 1);
  cl_assert(prv_compare_points(&center, s_stroke_points, s_path_num_points));
  cl_assert_equal_p(s_fill_points, NULL);

  prv_reset();
  // restore radius and set hidden - nothing should be drawn
  gdraw_command_set_radius(command, 300);
  gdraw_command_set_hidden(command, true);
  gdraw_command_draw(NULL, command);
  cl_assert_equal_i(s_stroke_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_fill_color.argb, GColorClearARGB8);
  cl_assert_equal_i(s_stroke_width, 0);
  cl_assert_equal_i(s_path_num_points, 0);
  cl_assert_equal_b(s_radius, 0);
  cl_assert_equal_i(s_circle_fill_count, 0);
  cl_assert_equal_i(s_circle_stroke_count, 0);
  cl_assert_equal_p(s_stroke_points, NULL);
  cl_assert_equal_p(s_fill_points, NULL);

  free(command);
}

static GDrawCommandList *prv_create_command_list_3(void) {
  GDrawCommandList *command_list = malloc(sizeof(GDrawCommandList) + (3 * sizeof(GDrawCommand)) +
      (sizeof(GPoint) * 6));
  *command_list = (GDrawCommandList) {
    .num_commands = 3
  };
  GDrawCommand *command;
  command = gdraw_command_list_get_command(command_list, 0);
  *command = (GDrawCommand) {
    .type = GDrawCommandTypePath,
    .hidden = false,
    .stroke_color = GColorRed,
    .stroke_width = 1,
    .fill_color = GColorBlue,
    .path_open = false,
    .num_points = 2,
  };
  GPoint points1[] = { { 3, 97 }, {5, 5} };
  memcpy(command_list->commands[0].points, points1, sizeof(points1));

  command = gdraw_command_list_get_command(command_list, 1);
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

  command = gdraw_command_list_get_command(command_list, 2);
  *command = (GDrawCommand) {
    .type = GDrawCommandTypePath,
    .hidden = false,
    .stroke_color = GColorGreen,
    .stroke_width = 1,
    .fill_color = GColorPurple,
    .path_open = false,
    .num_points = 3,
  };
  GPoint points2[] = { { 6, 7 }, {5, 5}, { 0, 0 } };
  memcpy(command->points, points2, sizeof(points2));

  return command_list;
}

void test_gdraw_command__draw_command_list(void) {
  GDrawCommandList *command_list = malloc(sizeof(GDrawCommandList) + sizeof(GDrawCommand) +
      (sizeof(GPoint) * 2));
  *command_list = (GDrawCommandList) {
    .num_commands = 1
  };
  command_list->commands[0] = (GDrawCommand) {
    .type = GDrawCommandTypePath,
    .hidden = false,
    .stroke_color = GColorRed,
    .stroke_width = 1,
    .fill_color = GColorBlue,
    .path_open = false,
    .num_points = 2,
  };
  GPoint points1[] = { { 3, 97 }, {5, 5} };
  memcpy(command_list->commands[0].points, points1, sizeof(points1));

  GContext *ctx = (GContext *)123; // just a fake internal guard != NULL

  gdraw_command_list_draw(ctx, command_list);
  cl_assert_equal_i(s_path_stroke_count, 1);
  cl_assert_equal_i(s_path_fill_count, 1);
  cl_assert_equal_i(s_circle_stroke_count, 0);
  cl_assert_equal_i(s_circle_fill_count, 0);

  prv_reset();
  free(command_list);

  command_list = prv_create_command_list_3();

  gdraw_command_list_draw(ctx, command_list);
  cl_assert_equal_i(s_path_stroke_count, 2);
  cl_assert_equal_i(s_path_fill_count, 2);
  cl_assert_equal_i(s_circle_stroke_count, 1);
  cl_assert_equal_i(s_circle_fill_count, 1);

  prv_reset();
  gdraw_command_list_get_command(command_list, 2)->hidden = true;
  gdraw_command_list_draw(ctx, command_list);
  cl_assert_equal_i(s_path_stroke_count, 1);
  cl_assert_equal_i(s_path_fill_count, 1);
  cl_assert_equal_i(s_circle_stroke_count, 1);
  cl_assert_equal_i(s_circle_fill_count, 1);

  prv_reset();
  gdraw_command_list_get_command(command_list, 0)->hidden = true;
  gdraw_command_list_draw(ctx, command_list);
  cl_assert_equal_i(s_path_stroke_count, 0);
  cl_assert_equal_i(s_path_fill_count, 0);
  cl_assert_equal_i(s_circle_stroke_count, 1);
  cl_assert_equal_i(s_circle_fill_count, 1);

  prv_reset();
  gdraw_command_list_get_command(command_list, 1)->hidden = true;
  gdraw_command_list_draw(ctx, command_list);
  cl_assert_equal_i(s_path_stroke_count, 0);
  cl_assert_equal_i(s_path_fill_count, 0);
  cl_assert_equal_i(s_circle_stroke_count, 0);
  cl_assert_equal_i(s_circle_fill_count, 0);

  free(command_list);
}

void test_gdraw_command__validate_list(void) {
  GDrawCommandList *command_list = prv_create_command_list_3();
  size_t size = sizeof(GDrawCommandList) + (3 * sizeof(GDrawCommand)) +
      (sizeof(GPoint) * 6);
  cl_assert(gdraw_command_list_validate(command_list, size));

  command_list->num_commands = 4;
  cl_assert(!gdraw_command_list_validate(command_list, size));
  command_list->num_commands = 3;
  GDrawCommand *command = gdraw_command_list_get_command(command_list, 0);
  command->num_points = 0;
  cl_assert(!gdraw_command_list_validate(command_list, size));
  command->num_points = 2;
  command = gdraw_command_list_get_command(command_list, 2);
  command->num_points = 4;
  cl_assert(!gdraw_command_list_validate(command_list, size));
  command->num_points = 3;
  command->type = GDrawCommandTypeCircle;
  cl_assert(!gdraw_command_list_validate(command_list, size));
  command->type = GDrawCommandTypePrecisePath;
  cl_assert(gdraw_command_list_validate(command_list, size));

  free(command_list);
}

void test_gdraw_command__validate_image(void) {
  size_t size = sizeof(GDrawCommandImage) + (3 * sizeof(GDrawCommand)) +
      (sizeof(GPoint) * 6);

  GDrawCommandImage *image = malloc(size);
  GDrawCommandList *command_list = prv_create_command_list_3();
  memcpy(&image->command_list, command_list, sizeof(GDrawCommandList) + (3 * sizeof(GDrawCommand)) +
      (sizeof(GPoint) * 6));
  free(command_list);

  image->version = 1;
  image->size = GSize(20, 20);
  cl_assert(gdraw_command_image_validate(image, size));
  cl_assert(!gdraw_command_image_validate(image, size - 1));
  cl_assert(!gdraw_command_image_validate(image, size + 1));

  cl_assert_equal_i(size, gdraw_command_image_get_data_size(image));

  image->version = 2;
  cl_assert(!gdraw_command_image_validate(image, size));

  free(image);
}

void test_gdraw_command__clone_image(void) {
  cl_assert_equal_p(gdraw_command_image_clone(NULL), NULL);

  size_t size = sizeof(GDrawCommandImage) + (3 * sizeof(GDrawCommand)) +
                (sizeof(GPoint) * 6);

  GDrawCommandImage *image = malloc(size);
  memset(image, 0, size);

  GDrawCommandList *command_list = prv_create_command_list_3();
  memcpy(&image->command_list, command_list, sizeof(GDrawCommandList) + (3 * sizeof(GDrawCommand)) +
                                             (sizeof(GPoint) * 6));
  free(command_list);

  image->version = 1;
  image->size = GSize(20, 20);

  GDrawCommandImage *clone = gdraw_command_image_clone(image);
  cl_assert(clone != image);
  cl_assert_equal_i(gdraw_command_image_get_data_size(clone), size);
  cl_assert_equal_i(0, memcmp(clone, image, size));

  free(clone);
  free(image);
}

void test_gdraw_command__draw_image(void) {
  GDrawCommandImage *image = malloc(sizeof(GDrawCommandImage) + sizeof(GDrawCommand) +
      (sizeof(GPoint) * 2));
  GDrawCommandList *command_list = &image->command_list;
  *command_list = (GDrawCommandList) {
    .num_commands = 1
  };
  GDrawCommand *command;
  command = gdraw_command_list_get_command(command_list, 0);
  *command = (GDrawCommand) {
    .type = GDrawCommandTypePath,
    .hidden = false,
    .stroke_color = GColorRed,
    .stroke_width = 1,
    .fill_color = GColorBlue,
    .path_open = true,
    .num_points = 2,
  };
  GPoint points[] = {{ 6, 1 }, {5, -5} };
  memcpy(command_list->commands[0].points, points, sizeof(points));

  GContext *ctx = (GContext *)123; // just a fake internal guard != NULL

  gdraw_command_image_draw(ctx, image, GPoint(0, 0));
  cl_assert_equal_i(s_path_num_points, 2);
  cl_assert(prv_compare_points(points, s_stroke_points, s_path_num_points));
  cl_assert(prv_compare_points(points, s_fill_points, s_path_num_points));

  prv_reset();
  gdraw_command_image_draw(ctx, image, GPoint(-1, 1));
  cl_assert_equal_i(s_path_num_points, 2);
  points[0] = GPoint(5, 2);
  points[1] = GPoint(4, -4);
  cl_assert(prv_compare_points(points, s_stroke_points, s_path_num_points));
  cl_assert(prv_compare_points(points, s_fill_points, s_path_num_points));

  free(image);
}

void test_gdraw_command__draw_frame(void) {
  GDrawCommandFrame *frame = malloc(sizeof(GDrawCommandFrame) + sizeof(GDrawCommand) +
      (sizeof(GPoint) * 2));
  GDrawCommandList *command_list = &frame->command_list;
  *command_list = (GDrawCommandList) {
    .num_commands = 1
  };
  GDrawCommand *command = gdraw_command_list_get_command(command_list, 0);
  *command = (GDrawCommand) {
    .type = GDrawCommandTypePath,
    .hidden = false,
    .stroke_color = GColorRed,
    .stroke_width = 1,
    .fill_color = GColorBlue,
    .path_open = true,
    .num_points = 2,
  };
  GPoint points[] = {{ 1, 1 }, {2, -2} };
  memcpy(command_list->commands[0].points, points, sizeof(points));

  GContext *ctx = (GContext *)123; // just a fake internal guard != NULL

  gdraw_command_frame_draw(ctx, NULL, frame, GPoint(0, 0));
  cl_assert_equal_i(s_path_num_points, 2);
  cl_assert(prv_compare_points(points, s_stroke_points, s_path_num_points));
  cl_assert(prv_compare_points(points, s_fill_points, s_path_num_points));

  prv_reset();
  gdraw_command_frame_draw(ctx, NULL, frame, GPoint(-1, 1));
  cl_assert_equal_i(s_path_num_points, 2);
  points[0] = GPoint(0, 2);
  points[1] = GPoint(1, -1);
  cl_assert(prv_compare_points(points, s_stroke_points, s_path_num_points));
  cl_assert(prv_compare_points(points, s_fill_points, s_path_num_points));

  free(frame);
}

static int s_iterations = 0;
static bool prv_iterate(GDrawCommand *command, uint32_t index, void *context) {
  s_iterations++;
  return true;
}

void test_gdraw_command__iterate(void) {
  GDrawCommandList *command_list = prv_create_command_list_3();
  void *end  = gdraw_command_list_iterate_private(command_list, prv_iterate, NULL);
  cl_assert_equal_i(s_iterations, 3);
  GDrawCommand *command = gdraw_command_list_get_command(command_list, 2);
  void *expected_end = command->points + command->num_points;
  cl_assert_equal_p(end, expected_end);

  free(command_list);
}

typedef struct {
  GDrawCommandProcessor processor;
  GColor stroke_color;
} SetStrokeColorProcessor;

static void prv_set_stroke_color(GDrawCommandProcessor *processor,
                                 GDrawCommand *processed_command,
                                 const size_t processed_command_max_size,
                                 const GDrawCommandList* list,
                                 const GDrawCommand *command) {
  SetStrokeColorProcessor *stroke_processor = (SetStrokeColorProcessor *)processor;
  gdraw_command_set_stroke_color(processed_command, stroke_processor->stroke_color);
}

void test_gdraw_command__draw_command_list_processed(void) {
  GDrawCommandList *command_list = prv_create_command_list_3();
  GColor stroke_color = GColorTiffanyBlue;
  SetStrokeColorProcessor stroke_processor = {
    .processor = {
      .command = prv_set_stroke_color,
    },
    .stroke_color = stroke_color,
  };

  // ctx can't be null pass in random pointer to satisfy function
  GContext ctx;
  gdraw_command_list_draw_processed(&ctx, command_list, (GDrawCommandProcessor *)&stroke_processor);
  cl_assert_equal_i(s_stroke_color.argb, GColorTiffanyBlueARGB8);

  free(command_list);
}

void test_gdraw_command__get_max_command_size_in_list(void) {
  GDrawCommandList *command_list = prv_create_command_list_3();

  // The third command in the list is the largest so it's size should be returned by
  // prv_get_list_max_command_size
  cl_assert_equal_i(gdraw_command_get_data_size(gdraw_command_list_get_command(command_list, 2)),
                    prv_get_list_max_command_size(command_list));

  free(command_list);
}


