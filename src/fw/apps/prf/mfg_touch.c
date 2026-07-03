/* SPDX-FileCopyrightText: 2025 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */


#include "applib/app.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "applib/tick_timer_service.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window.h"
#include "apps/prf/mfg_test_result.h"
#include "kernel/event_loop.h"
#include "kernel/pbl_malloc.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"
#include "applib/touch_service.h"
#include "pbl/services/light.h"
#include "pbl/services/touch/touch.h"
#include "pbl/util/trig.h"

#include <stdio.h>

#define TOUCH_SUPPORT_DEBUG    0

#define TEST_TIMEOUT_S (30)
#define WINDOW_POP_TIME_S (3)

#if PBL_ROUND
// Concentric rings joined by radial connectors: constant-radius arcs are
// easy to follow on a circular panel and cover most of its surface. The
// display splits into equal-width ring lanes around a central hub that
// holds the status text; each guide circle runs along the middle of its
// lane, with wall lines on the lane boundaries.
#define NUM_RINGS (2)
#define HUB_RADIUS_PX (40)
#define RING_LANE_WIDTH_PX ((PBL_DISPLAY_WIDTH / 2 - HUB_RADIUS_PX) / NUM_RINGS)
// Path vertex spacing along the arcs; gap left where a ring hands over to
// the radial connector towards the next ring
#define RING_ARC_STEP_PX (25)
#define RING_GAP_ARC_PX (40)
#define MAX_DOTS (72)
// Wall lines sit on the lane boundaries: allow deviations right up to them
#define PATH_TOLERANCE_PX (RING_LANE_WIDTH_PX / 2)
#else
#define GRID_ROWS (5)
#define GRID_COLS (5)
#define GRID_SPACING_X (PBL_DISPLAY_WIDTH / GRID_COLS)
#define GRID_SPACING_Y (PBL_DISPLAY_HEIGHT / GRID_ROWS)
#define MAX_DOTS (GRID_ROWS * GRID_COLS)
// Wall lines sit halfway between lanes: allow deviations right up to them
#define PATH_TOLERANCE_PX (GRID_SPACING_X / 2)
#endif

#define DOT_RADIUS_PX (4)
#define DOT_CAPTURE_RADIUS_PX (14)
// Segments ahead of the current one a sample may match, so a fast drag with
// sparse samples cannot stall progress at a skipped dot. Kept well below the
// ring/row separation so a stroke cannot tunnel to another path section.
#define PATH_LOOKAHEAD_SEGMENTS (3)
#define TRACE_MIN_DIST_PX (3)
#define MAX_TRACE_POINTS (640)

typedef struct {
  Window window;
  GPoint dots[MAX_DOTS];
  uint8_t num_dots;
#if PBL_ROUND
  int16_t ring_radius[NUM_RINGS];
  int32_t ring_angle[NUM_RINGS];
  int32_t ring_sweep[NUM_RINGS];
  uint8_t ring_first[NUM_RINGS];
#endif
  GPoint trace[MAX_TRACE_POINTS];
  uint16_t trace_len;
  uint8_t next_dot;
  uint8_t best_progress;
  bool tracking;
  TextLayer status;
  char status_string[35];
  uint32_t seconds_remaining;
  bool test_complete;
} AppData;

static void prv_complete_test(AppData *data, bool passed) {
  data->test_complete = true;
  data->seconds_remaining = WINDOW_POP_TIME_S;

  mfg_test_result_report(MfgTestId_Touch, passed, data->best_progress);

  sniprintf(data->status_string, sizeof(data->status_string), "%s", passed ? "PASS!" : "FAIL!");
  text_layer_set_text(&data->status, data->status_string);
  // The status overlay is transparent: repaint the whole window under it
  layer_mark_dirty(&data->window.layer);
}

static void prv_handle_second_tick(struct tm *tick_time, TimeUnits units_changed) {
  AppData *data = app_state_get_user_data();

  if (data->test_complete) {
    if (--data->seconds_remaining == 0) {
      app_window_stack_pop(true);
    }
    return;
  }

  if (data->seconds_remaining == 0) {
    prv_complete_test(data, false);
  } else {
    sniprintf(data->status_string, sizeof(data->status_string),
              "%" PRIu32 "s", data->seconds_remaining);
    text_layer_set_text(&data->status, data->status_string);
    layer_mark_dirty(&data->window.layer);
    data->seconds_remaining--;
  }
}

static uint32_t prv_dist_sq(GPoint a, GPoint b) {
  int32_t dx = a.x - b.x;
  int32_t dy = a.y - b.y;
  return dx * dx + dy * dy;
}

static uint32_t prv_dist_sq_to_segment(GPoint p, GPoint a, GPoint b) {
  int32_t abx = b.x - a.x;
  int32_t aby = b.y - a.y;
  int32_t apx = p.x - a.x;
  int32_t apy = p.y - a.y;
  int32_t len_sq = abx * abx + aby * aby;
  int32_t t_num = apx * abx + apy * aby;
  int32_t cx = 0;
  int32_t cy = 0;

  if (len_sq > 0 && t_num >= len_sq) {
    cx = abx;
    cy = aby;
  } else if (len_sq > 0 && t_num > 0) {
    cx = (abx * t_num) / len_sq;
    cy = (aby * t_num) / len_sq;
  }

  int32_t dx = apx - cx;
  int32_t dy = apy - cy;
  return dx * dx + dy * dy;
}

static void prv_reset_stroke(AppData *data) {
  // Keep the trace on screen so the operator can see where the stroke went
  // wrong; it is discarded when a new stroke starts
  data->next_dot = 0;
  data->tracking = false;
}

static void prv_trace_append(AppData *data, GPoint p) {
  if (data->trace_len == MAX_TRACE_POINTS) {
    return;
  }
  if (data->trace_len > 0 &&
      prv_dist_sq(p, data->trace[data->trace_len - 1]) <
          TRACE_MIN_DIST_PX * TRACE_MIN_DIST_PX) {
    return;
  }
  data->trace[data->trace_len++] = p;
}

//! Draw the guide path in the current stroke color/width: arcs plus radial
//! connectors on round, the snake polyline on rect
static void prv_draw_guide_path(GContext *ctx, AppData *data) {
#if PBL_ROUND
  for (uint8_t r = 0; r < NUM_RINGS; r++) {
    GRect rect = GRect(PBL_DISPLAY_WIDTH / 2 - data->ring_radius[r],
                       PBL_DISPLAY_HEIGHT / 2 - data->ring_radius[r],
                       2 * data->ring_radius[r] + 1, 2 * data->ring_radius[r] + 1);
    // graphics_draw_arc measures angles from the top of the circle
    graphics_draw_arc(ctx, rect, GOvalScaleModeFitCircle,
                      data->ring_angle[r] + TRIG_MAX_ANGLE / 4,
                      data->ring_angle[r] + data->ring_sweep[r] + TRIG_MAX_ANGLE / 4);
  }
  for (uint8_t r = 1; r < NUM_RINGS; r++) {
    graphics_draw_line(ctx, data->dots[data->ring_first[r] - 1],
                       data->dots[data->ring_first[r]]);
  }
#else
  for (uint8_t i = 0; i < data->num_dots - 1; i++) {
    graphics_draw_line(ctx, data->dots[i], data->dots[i + 1]);
  }
#endif
}

//! Draw a single maze-style wall between each pair of adjacent lanes, with a
//! gap where the path crosses over
static void prv_draw_walls(GContext *ctx, AppData *data) {
#if PBL_ROUND
  for (uint8_t r = 1; r < NUM_RINGS; r++) {
    int16_t wall_radius = (data->ring_radius[r - 1] + data->ring_radius[r]) / 2;
    GRect rect = GRect(PBL_DISPLAY_WIDTH / 2 - wall_radius,
                       PBL_DISPLAY_HEIGHT / 2 - wall_radius,
                       2 * wall_radius + 1, 2 * wall_radius + 1);
    // Gate opening centered on the radial connector
    int32_t circumference = (2 * wall_radius * 6283) / 2000;
    int32_t half_gate = (TRIG_MAX_ANGLE * PATH_TOLERANCE_PX) / circumference;
    graphics_draw_arc(ctx, rect, GOvalScaleModeFitCircle,
                      data->ring_angle[r] + half_gate + TRIG_MAX_ANGLE / 4,
                      data->ring_angle[r] - half_gate + TRIG_MAX_ANGLE +
                          TRIG_MAX_ANGLE / 4);
  }
  // Central hub wall around the status text, bounding the innermost lane
  {
    int16_t hub_radius = data->ring_radius[NUM_RINGS - 1] - RING_LANE_WIDTH_PX / 2;
    GRect rect = GRect(PBL_DISPLAY_WIDTH / 2 - hub_radius,
                       PBL_DISPLAY_HEIGHT / 2 - hub_radius,
                       2 * hub_radius + 1, 2 * hub_radius + 1);
    graphics_draw_arc(ctx, rect, GOvalScaleModeFitCircle, 0, TRIG_MAX_ANGLE);
  }
#else
  for (uint8_t k = 1; k < GRID_ROWS; k++) {
    int16_t y = GRID_SPACING_Y / 2 + (k - 1) * GRID_SPACING_Y + GRID_SPACING_Y / 2;
    if (k % 2 == 1) {
      // Rows connect on the right: wall spans from the left edge
      graphics_draw_line(ctx, GPoint(0, y),
                         GPoint(PBL_DISPLAY_WIDTH - GRID_SPACING_X, y));
    } else {
      graphics_draw_line(ctx, GPoint(GRID_SPACING_X, y),
                         GPoint(PBL_DISPLAY_WIDTH - 1, y));
    }
  }
#endif
}

static void prv_update_proc(struct Layer *layer, GContext* ctx) {
  AppData *data = app_state_get_user_data();

  // This update proc replaces the default window one: clear the background
  // ourselves so stale trace pixels do not linger
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, &layer->bounds);

  // Keep frames cheap: touch samples arrive faster than antialiased arcs
  // can be rendered, which would back up the app event queue
  graphics_context_set_antialiased(ctx, false);

  graphics_context_set_stroke_width(ctx, 3);

  graphics_context_set_stroke_color(ctx, GColorMelon);
  prv_draw_walls(ctx, data);

  graphics_context_set_stroke_color(ctx, GColorLightGray);
  prv_draw_guide_path(ctx, data);

#if PBL_ROUND
  // Ring vertices are dense; mark every other one
  for (uint8_t i = 0; i < data->num_dots; i += 2) {
    graphics_context_set_fill_color(ctx, i < data->next_dot ? GColorGreen : GColorBlack);
    graphics_fill_circle(ctx, data->dots[i], DOT_RADIUS_PX);
  }
#else
  for (uint8_t i = 0; i < data->num_dots; i++) {
    graphics_context_set_fill_color(ctx, i < data->next_dot ? GColorGreen : GColorBlack);
    graphics_fill_circle(ctx, data->dots[i], DOT_RADIUS_PX);
  }
#endif

  graphics_context_set_stroke_color(ctx, GColorBlack);
  for (uint16_t i = 1; i < data->trace_len; i++) {
    graphics_draw_line(ctx, data->trace[i - 1], data->trace[i]);
  }
}

static void prv_touch_event_handler(const TouchEvent *event, void *context) {
  AppData *data = app_state_get_user_data();

  if (data->test_complete) {
    return;
  }

  GPoint p = GPoint(event->x, event->y);

#if TOUCH_SUPPORT_DEBUG
  PBL_LOG_INFO("type:%d x:%d y:%d", event->type, event->x, event->y);
#endif

  if (event->type == TouchEvent_Liftoff) {
    // The path must be completed in a single stroke
    prv_reset_stroke(data);
    layer_mark_dirty(&data->window.layer);
    return;
  }

  if (!data->tracking) {
    // Stroke starts anywhere within the corridor mouth around the first dot
    if (prv_dist_sq(p, data->dots[0]) <=
        PATH_TOLERANCE_PX * PATH_TOLERANCE_PX) {
      data->trace_len = 0;
      data->tracking = true;
      data->next_dot = 1;
      prv_trace_append(data, p);
      layer_mark_dirty(&data->window.layer);
    }
    return;
  }

  // Match the sample against the nearest guide segment, from the previous
  // one up to a small lookahead past the current one
  uint32_t best_dist_sq = UINT32_MAX;
  uint8_t best_end = data->next_dot;
  uint8_t first_end = (data->next_dot >= 2) ? data->next_dot - 1 : data->next_dot;
  for (uint8_t end = first_end;
       end < data->num_dots && end <= data->next_dot + PATH_LOOKAHEAD_SEGMENTS; end++) {
    uint32_t dist_sq = prv_dist_sq_to_segment(p, data->dots[end - 1], data->dots[end]);
    if (dist_sq < best_dist_sq) {
      best_dist_sq = dist_sq;
      best_end = end;
    }
  }

  if (best_dist_sq > PATH_TOLERANCE_PX * PATH_TOLERANCE_PX) {
    // Off the corridor (finger slip or distorted coordinates): reset stroke
    prv_reset_stroke(data);
    layer_mark_dirty(&data->window.layer);
    return;
  }

  // A sample on segment (best_end - 1 -> best_end) means all prior dots
  // were passed; the endpoint itself is captured within its radius
  uint8_t prev_next_dot = data->next_dot;
  uint16_t prev_trace_len = data->trace_len;
  if (best_end > data->next_dot) {
    data->next_dot = best_end;
  }
  while (data->next_dot < data->num_dots &&
         prv_dist_sq(p, data->dots[data->next_dot]) <=
             DOT_CAPTURE_RADIUS_PX * DOT_CAPTURE_RADIUS_PX) {
    data->next_dot++;
  }
  if (data->next_dot > data->best_progress) {
    data->best_progress = data->next_dot;
  }

  prv_trace_append(data, p);
  // Only request a redraw when something visible changed
  if (data->next_dot != prev_next_dot || data->trace_len != prev_trace_len) {
    layer_mark_dirty(&data->window.layer);
  }

  if (data->next_dot == data->num_dots) {
    prv_complete_test(data, true);
  }
}

static void prv_init_dots(AppData *data) {
#if PBL_ROUND
  GPoint center = GPoint(PBL_DISPLAY_WIDTH / 2, PBL_DISPLAY_HEIGHT / 2);
  // Start at the top; each ring sweeps a full circle minus a gap, then the
  // path drops radially to the next ring
  int32_t angle = 3 * TRIG_MAX_ANGLE / 4;
  data->num_dots = 0;

  for (uint8_t ring = 0; ring < NUM_RINGS; ring++) {
    int32_t radius =
        PBL_DISPLAY_WIDTH / 2 - RING_LANE_WIDTH_PX * ring - RING_LANE_WIDTH_PX / 2;
    int32_t circumference = (2 * radius * 6283) / 2000;  // 2 * pi * r
    int32_t sweep = (TRIG_MAX_ANGLE * (circumference - RING_GAP_ARC_PX)) / circumference;
    uint8_t segments = (circumference - RING_GAP_ARC_PX) / RING_ARC_STEP_PX;

    data->ring_radius[ring] = radius;
    data->ring_angle[ring] = angle;
    data->ring_sweep[ring] = sweep;
    data->ring_first[ring] = data->num_dots;

    for (uint8_t i = 0; i <= segments && data->num_dots < MAX_DOTS; i++) {
      int32_t a = angle + (sweep * i) / segments;
      data->dots[data->num_dots].x = center.x + (radius * cos_lookup(a)) / TRIG_MAX_RATIO;
      data->dots[data->num_dots].y = center.y + (radius * sin_lookup(a)) / TRIG_MAX_RATIO;
      data->num_dots++;
    }
    angle += sweep;
  }
#else
  int16_t spacing_x = PBL_DISPLAY_WIDTH / GRID_COLS;
  int16_t spacing_y = PBL_DISPLAY_HEIGHT / GRID_ROWS;

  // Boustrophedon (snake) ordering: even rows left-to-right, odd rows right-to-left
  for (uint8_t row = 0; row < GRID_ROWS; row++) {
    for (uint8_t col = 0; col < GRID_COLS; col++) {
      uint8_t eff_col = (row % 2 == 0) ? col : GRID_COLS - 1 - col;
      data->dots[row * GRID_COLS + col].x = spacing_x / 2 + eff_col * spacing_x;
      data->dots[row * GRID_COLS + col].y = spacing_y / 2 + row * spacing_y;
    }
  }
  data->num_dots = GRID_ROWS * GRID_COLS;
#endif
}

static void prv_handle_init(void) {
  AppData *data = app_malloc_check(sizeof(AppData));
  *data = (AppData) {
    .seconds_remaining = TEST_TIMEOUT_S,
  };

  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, "");
  window_set_fullscreen(window, true);
  Layer *layer = window_get_root_layer(window);
  layer_set_update_proc(layer, prv_update_proc);
  app_window_stack_push(window, true /* Animated */);

  prv_init_dots(data);
  layer_mark_dirty(&data->window.layer);

  TextLayer *status = &data->status;
  // Centered overlay; kept small so it covers as little of the path as possible
  text_layer_init(status,
                  &GRect(PBL_DISPLAY_WIDTH / 2 - 34, PBL_DISPLAY_HEIGHT / 2 - 20, 68, 40));
  text_layer_set_font(status, fonts_get_system_font(FONT_KEY_GOTHIC_24));
  text_layer_set_text_alignment(status, GTextAlignmentCenter);
  text_layer_set_background_color(status, GColorWhite);
  layer_add_child(&window->layer, &status->layer);

  touch_service_subscribe(prv_touch_event_handler, NULL);

  tick_timer_service_subscribe(SECOND_UNIT, prv_handle_second_tick);
}

static void s_main(void) {
  light_enable(true);

  prv_handle_init();

  app_event_loop();

  touch_service_unsubscribe();
  light_enable(false);
}

const PebbleProcessMd* mfg_touch_app_get_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = &s_main,
    // UUID: a53e7d1c-d2ee-4592-96b9-5d33a46237db
    .common.uuid = { 0xa5, 0x3e, 0x7d, 0x1c, 0xd2, 0xee, 0x45, 0x92,
                     0x96, 0xb9, 0x5d, 0x33, 0xa4, 0x62, 0x37, 0xdb },
    .name = "MfgTouch",
  };
  return (const PebbleProcessMd*) &s_app_info;
}
