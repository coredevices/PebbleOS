/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "pebble_shapes.h"

#include "applib/app.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/gpath.h"
#include "applib/graphics/gtypes.h"
#include "applib/graphics/gtransform.h"
#include "applib/graphics/text.h"
#include "util/trig.h"
#include "applib/ui/action_bar_layer.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/window.h"
#include "kernel/pbl_malloc.h"
#include "process_state/app_state/app_state.h"
#include "system/logging.h"

typedef enum {
  POINT,
  LINE,
  SQUARE,
  RECTANGLE,
  RECTANGLE_ROUND,
  CIRCLE,
  GPATH_TRIANGLE,
  GPATH_OPEN_BUCKET,
  MAX_SHAPES
} DrawShape;

#define NUM_SHAPES MAX_SHAPES
static GColor s_display_colors[NUM_SHAPES];

#define MAX_STROKE_WIDTH 20

static const GPathInfo s_triangle_path = {
  3,
  (GPoint[]) {
    { -10, 0 },
    { 0, 10 },
    { 10, 0 }
  }
};

static const GPathInfo s_bucket_path = {
  4,
  (GPoint[]) {
    { -10, 0 },
    { -10, 30 },
    { 10, 30 },
    { 10, 0 }
  }
};

static const int TARGET_FPS = 40;
static const int PIXEL_SPEED_PER_FRAME = 2;

#define ANGLE_DEGREES_TO_TRIG_ANGLE(angle) (((angle % 360) * TRIG_MAX_ANGLE) / 360)
#define MAX_SCALE 10

typedef enum {
  APP_STATE_FILL_NON_AA,
  APP_STATE_FILL_AA,
  APP_STATE_DRAW_NON_AA_NO_SW,
  APP_STATE_DRAW_AA_NO_SW,
  APP_STATE_DRAW_NON_AA_SW,
  APP_STATE_DRAW_AA_SW,

  // Add more above
  APP_STATE_NUM_STATES
} AppStateIndex;

typedef struct AppData {
  Window window;

  // Point properties
  GPoint point_p0;
  int16_t point_velocity_x;
  int16_t point_velocity_y;

  // Line properties
  GPoint line_p0;
  GPoint line_p1;
  int16_t line_velocity_x;
  int16_t line_velocity_y;

  // Square properties
  GRect square;
  int16_t square_velocity_x;
  int16_t square_velocity_y;

  // Rectangle properties
  GRect rect;
  int16_t rect_velocity_x;
  int16_t rect_velocity_y;

  // Rectangle Round properties
  GRect rectr;
  uint16_t rectr_radius;
  uint16_t rectr_corners_index;
  int16_t rectr_velocity_x;
  int16_t rectr_velocity_y;

  // Circle properties
  GPoint circle_origin;
  uint16_t circle_radius;
  int16_t circle_velocity_x;
  int16_t circle_velocity_y;
  GColor circle_color;

  // Triangle
  GPath *triangle;
  GPoint triangle_offset;
  int16_t triangle_velocity_x;
  int16_t triangle_velocity_y;

  // Bucket
  GPath *bucket;
  GPoint bucket_offset;
  int16_t bucket_velocity_x;
  int16_t bucket_velocity_y;

  // Generic properties that can be changed as property_index changes
  bool fill;
  int16_t color_index;

  int64_t time_started;
  uint32_t rendered_frames;

  bool moving;
  AppStateIndex state_index;
  int16_t stroke_width;
  bool antialiased;
} AppData;


static void log_state(AppData *data) {
  switch (data->state_index) {
    case APP_STATE_FILL_NON_AA:
      PBL_LOG(LOG_LEVEL_DEBUG, "State: Fill Non-Antialiased; SW: N/A (but currently: %d)",
              data->stroke_width);
      break;
    case APP_STATE_FILL_AA:
      PBL_LOG(LOG_LEVEL_DEBUG, "State: Fill Antialiased; SW: N/A (but currently: %d)",
              data->stroke_width);
      break;
    case APP_STATE_DRAW_NON_AA_NO_SW:
      PBL_LOG(LOG_LEVEL_DEBUG, "State: Draw Non-Antialiased; SW: N/A (but currently: %d)",
              data->stroke_width);
      break;
    case APP_STATE_DRAW_AA_NO_SW:
      PBL_LOG(LOG_LEVEL_DEBUG, "State: Draw Antialiased; SW: N/A (but currently: %d)",
              data->stroke_width);
      break;
    case APP_STATE_DRAW_NON_AA_SW:
      PBL_LOG(LOG_LEVEL_DEBUG, "State: Draw Non-Antialiased; SW: %d", data->stroke_width);
      break;
    case APP_STATE_DRAW_AA_SW:
      PBL_LOG(LOG_LEVEL_DEBUG, "State: Draw Antialiased; SW: %d", data->stroke_width);
      break;
    default:
      PBL_LOG(LOG_LEVEL_DEBUG, "Unknown State");
      break;
  }
}

static bool stroke_width_enabled(AppStateIndex state_index) {
  return ((state_index == APP_STATE_DRAW_AA_SW) || (state_index == APP_STATE_DRAW_NON_AA_SW));
}

static void update_state(AppData *data, AppStateIndex state_index) {
  data->state_index = state_index;
  data->fill = ((data->state_index == APP_STATE_FILL_NON_AA) ||
                (data->state_index == APP_STATE_FILL_AA));
  data->antialiased = ((data->state_index == APP_STATE_DRAW_AA_NO_SW) ||
                       (data->state_index == APP_STATE_DRAW_AA_SW) ||
                       (data->state_index == APP_STATE_FILL_AA));
}

static void back_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = app_state_get_user_data();

  update_state(data, ((data->state_index - 1) % APP_STATE_NUM_STATES));
  log_state(data);
}

static void up_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = app_state_get_user_data();

  if (stroke_width_enabled(data->state_index)) {
    data->stroke_width++;
    if (data->stroke_width >= MAX_STROKE_WIDTH) {
      data->stroke_width = 1;
    }
  }
  log_state(data);
}

static void select_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = app_state_get_user_data();

  update_state(data, ((data->state_index + 1) % APP_STATE_NUM_STATES));
  log_state(data);
}

static void down_handler(ClickRecognizerRef recognizer, void *context) {
  AppData *data = app_state_get_user_data();

  data->moving = !data->moving;

  log_state(data);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_BACK, back_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_handler);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_handler);
}

static void prv_move_shape(AppData *data) {
  for (uint8_t shape = POINT; shape < MAX_SHAPES; shape++) {
    if (shape == POINT) {
      // Move the point 4*X per Y
      data->point_p0.x += (data->point_velocity_x * PIXEL_SPEED_PER_FRAME * 4);
      if (data->point_p0.x < 0 || data->point_p0.x > data->window.layer.bounds.size.w) {
        data->point_velocity_x = data->point_velocity_x * -1;
      }

      data->point_p0.y += (data->point_velocity_y * PIXEL_SPEED_PER_FRAME);
      if (data->point_p0.y < 0 || data->point_p0.y > data->window.layer.bounds.size.h) {
        data->point_velocity_y = data->point_velocity_y * -1;
      }
    } else if (shape == LINE) {
      // Move the line 2*X per Y
      data->line_p0.x += (data->line_velocity_x * PIXEL_SPEED_PER_FRAME * 2);
      data->line_p1.x += (data->line_velocity_x * PIXEL_SPEED_PER_FRAME * 2);
      if (data->line_p0.x < 0 || data->line_p0.x > data->window.layer.bounds.size.w ||
          data->line_p1.x < 0 || data->line_p1.x > data->window.layer.bounds.size.w ) {
        data->line_velocity_x = data->line_velocity_x * -1;
      }

      data->line_p0.y += (data->line_velocity_y * PIXEL_SPEED_PER_FRAME);
      data->line_p1.y += (data->line_velocity_y * PIXEL_SPEED_PER_FRAME);
      if (data->line_p0.y < 0 || data->line_p0.y > data->window.layer.bounds.size.h ||
          data->line_p1.y < 0 || data->line_p1.y > data->window.layer.bounds.size.h ) {
        data->line_velocity_y = data->line_velocity_y * -1;
      }
    } else if (shape == SQUARE) {
      // Move the square X per Y
      data->square.origin.x += (data->square_velocity_x * PIXEL_SPEED_PER_FRAME);
      if (data->square.origin.x < 0 ||
          data->square.origin.x + data->square.size.w > data->window.layer.bounds.size.w) {
        data->square_velocity_x = data->square_velocity_x * -1;
      }

      data->square.origin.y += (data->square_velocity_y * PIXEL_SPEED_PER_FRAME);
      if (data->square.origin.y < 0 ||
          data->square.origin.y + data->square.size.h > data->window.layer.bounds.size.h) {
        data->square_velocity_y = data->square_velocity_y * -1;
      }
    } else if (shape == RECTANGLE) {
      // Move the rectangle X per 2*Y
      data->rect.origin.x += (data->rect_velocity_x * PIXEL_SPEED_PER_FRAME);
      if (data->rect.origin.x < 0 ||
          data->rect.origin.x + data->rect.size.w > data->window.layer.bounds.size.w) {
        data->rect_velocity_x = data->rect_velocity_x * -1;
      }

      data->rect.origin.y += (data->rect_velocity_y * PIXEL_SPEED_PER_FRAME * 2);
      if (data->rect.origin.y < 0 ||
          data->rect.origin.y + data->rect.size.h > data->window.layer.bounds.size.h) {
        data->rect_velocity_y = data->rect_velocity_y * -1;
      }
    } else if (shape == RECTANGLE_ROUND) {
      // Move rounded line X per 4*Y
      data->rectr.origin.x += (data->rectr_velocity_x * PIXEL_SPEED_PER_FRAME);
      if (data->rectr.origin.x < 0 ||
          data->rectr.origin.x + data->rectr.size.w > data->window.layer.bounds.size.w) {
        data->rectr_velocity_x = data->rectr_velocity_x * -1;
      }

      data->rectr.origin.y += (data->rectr_velocity_y * PIXEL_SPEED_PER_FRAME * 4);
      if (data->rectr.origin.y < 0 ||
          data->rectr.origin.y + data->rectr.size.h > data->window.layer.bounds.size.h) {
        data->rectr_velocity_y = data->rectr_velocity_y * -1;
      }
    } else if (shape == CIRCLE) {
      // Move the line X per Y
      data->circle_origin.x += (data->circle_velocity_x * PIXEL_SPEED_PER_FRAME);
      if (data->circle_origin.x - data->circle_radius < 0 ||
          data->circle_origin.x + data->circle_radius  > data->window.layer.bounds.size.w) {
        data->circle_velocity_x = data->circle_velocity_x * -1;
        data->circle_color.argb = (((data->circle_color.argb + 1) & 0x3F) | 0xC0);
      }

      data->circle_origin.y += (data->circle_velocity_y * PIXEL_SPEED_PER_FRAME);
      if (data->circle_origin.y - data->circle_radius < 0 ||
          data->circle_origin.y + data->circle_radius  > data->window.layer.bounds.size.h) {
        data->circle_velocity_y = data->circle_velocity_y * -1;
        data->circle_color.argb = (((data->circle_color.argb + 1) & 0x3F) | 0xC0);
      }
    } else if (shape == GPATH_TRIANGLE) {
      // Move the line 3*X per Y
      data->triangle_offset.x += (data->triangle_velocity_x * PIXEL_SPEED_PER_FRAME * 3);
      if (data->triangle_offset.x < 0 ||
          data->triangle_offset.x > data->window.layer.bounds.size.w) {
        data->triangle_velocity_x = data->triangle_velocity_x * -1;
      }

      data->triangle_offset.y += (data->triangle_velocity_y * PIXEL_SPEED_PER_FRAME);
      if (data->triangle_offset.y < 0 ||
          data->triangle_offset.y > data->window.layer.bounds.size.h) {
        data->triangle_velocity_y = data->triangle_velocity_y * -1;
      }
      gpath_move_to(data->triangle, data->triangle_offset);
    } else if (shape == GPATH_OPEN_BUCKET) {
      // Move the line 2*X per 3*Y
      data->bucket_offset.x += (data->bucket_velocity_x * PIXEL_SPEED_PER_FRAME * 2);
      if (data->bucket_offset.x < 0 ||
          data->bucket_offset.x > data->window.layer.bounds.size.w) {
        data->bucket_velocity_x = data->bucket_velocity_x * -1;
      }

      data->bucket_offset.y += (data->bucket_velocity_y * PIXEL_SPEED_PER_FRAME * 3);
      if (data->bucket_offset.y < 0 ||
          data->bucket_offset.y > data->window.layer.bounds.size.h) {
        data->bucket_velocity_y = data->bucket_velocity_y * -1;
      }
      gpath_move_to(data->bucket, data->bucket_offset);
    }
  }
}

static void draw_shape(GContext* ctx, AppData *data, DrawShape shape, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_context_set_stroke_color(ctx, color);
  if (data->fill) {
    if (shape == POINT) {
      graphics_draw_pixel(ctx, data->point_p0);
    } else if (shape == LINE) {
      graphics_draw_line(ctx, data->line_p0, data->line_p1);
    } else if (shape == SQUARE) {
      graphics_fill_rect(ctx, &data->square);
    } else if (shape == RECTANGLE) {
      graphics_fill_rect(ctx, &data->rect);
    } else if (shape == RECTANGLE_ROUND) {
      graphics_fill_round_rect(ctx, &data->rectr, data->rectr_radius, GCornersAll);
    } else if (shape == CIRCLE) {
      graphics_context_set_fill_color(ctx, data->circle_color);
      graphics_context_set_stroke_color(ctx, data->circle_color);
      graphics_fill_circle(ctx, data->circle_origin, data->circle_radius);
    } else if (shape == GPATH_TRIANGLE) {
      gpath_draw_filled(ctx, data->triangle);
    } else if (shape == GPATH_OPEN_BUCKET) {
      gpath_draw_filled(ctx, data->bucket);
    }
  } else {
    if (shape == POINT) {
      graphics_draw_pixel(ctx, data->point_p0);
    } else if (shape == LINE) {
      graphics_draw_line(ctx, data->line_p0, data->line_p1);
    } else if (shape == SQUARE) {
      graphics_draw_rect(ctx, &data->square);
    } else if (shape == RECTANGLE) {
      graphics_draw_rect(ctx, &data->rect);
    } else if (shape == RECTANGLE_ROUND) {
      graphics_draw_round_rect(ctx, &data->rectr, data->rectr_radius);
    } else if (shape == CIRCLE) {
      graphics_context_set_fill_color(ctx, data->circle_color);
      graphics_context_set_stroke_color(ctx, data->circle_color);
      graphics_draw_circle(ctx, data->circle_origin, data->circle_radius);
    } else if (shape == GPATH_TRIANGLE) {
      gpath_draw_outline(ctx, data->triangle);
    } else if (shape == GPATH_OPEN_BUCKET) {
      gpath_draw_outline_open(ctx, data->bucket);
    }
  }
}

static int64_t prv_time_64(void) {
  time_t s;
  uint16_t ms;
  rtc_get_time_ms(&s, &ms);
  return (int64_t)s * 1000 + ms;
}

static void layer_update_proc(Layer *layer, GContext* ctx) {
  AppData *data = app_state_get_user_data();

  graphics_context_set_fill_color(ctx, GColorBlack);
  if (stroke_width_enabled(data->state_index)) {
    graphics_context_set_stroke_width(ctx, data->stroke_width);
  } else {
    graphics_context_set_stroke_width(ctx, 1);
  }
  graphics_context_set_antialiased(ctx, data->antialiased);

  graphics_fill_rect(ctx, &layer->bounds);

  uint8_t color_index = 0;
  for (uint8_t shape_index = POINT; shape_index < MAX_SHAPES; shape_index++) {
    draw_shape(ctx, data, (DrawShape)shape_index, (GColor)s_display_colors[color_index++]);
  }


  if (data->rendered_frames == 0) {
    data->time_started = prv_time_64();
  } else {
    int64_t time_rendered = prv_time_64() - data->time_started;
    if ((data->rendered_frames % 64) == 0) {
      PBL_LOG(LOG_LEVEL_DEBUG, "## %d frames rendered", (int)data->rendered_frames);
      PBL_LOG(LOG_LEVEL_DEBUG, "## at %"PRIu32" FPS",
              (uint32_t)((uint64_t)data->rendered_frames*1000/time_rendered));
    }
  }
  data->rendered_frames++;
}


static void timer_callback(void *cb_data) {
  AppData *data = app_state_get_user_data();

  if (data->moving) {
    prv_move_shape(data);
  }

  layer_mark_dirty(&data->window.layer);

  app_timer_register(1000 / TARGET_FPS, timer_callback, NULL);
}

static void init(void) {
  AppData *data = task_malloc_check(sizeof(AppData));
  memset(data, 0x00, sizeof(AppData));

  s_display_colors[0] = GColorWhite;
  s_display_colors[1] = GColorRed;
  s_display_colors[2] = GColorGreen;
  s_display_colors[3] = GColorBlue;
  s_display_colors[4] = (GColor)((uint8_t)0b11111100);
  s_display_colors[5] = (GColor)((uint8_t)0b11001111);
  s_display_colors[6] = (GColor)((uint8_t)0b11110101);
  s_display_colors[7] = GColorWhite;


  app_state_set_user_data(data);

  Window *window = &data->window;
  window_init(window, WINDOW_NAME("Shapes"));
  window_set_user_data(window, data);
  window_set_fullscreen(window, true);
  layer_set_update_proc(&window->layer, layer_update_proc);

  window_set_click_config_provider(window, click_config_provider);

  const bool animated = true;
  app_window_stack_push(window, animated);

  // Initialize shapes

  // Point properties
  data->point_p0 = GPoint(1, 1);
  data->point_velocity_x = 1;
  data->point_velocity_y = 1;

  // Line properties
  data->line_p0 = GPoint(0, 0);
  data->line_p1 = GPoint(10, 10);
  data->line_velocity_x = 1;
  data->line_velocity_y = 1;

  // Square properties
  data->square = GRect(100, 50, 20, 20);
  data->square_velocity_x = 1;
  data->square_velocity_y = 1;

  // Rectangle properties
  data->rect = GRect(80, 0, 30, 50);
  data->rect_velocity_x = 1;
  data->rect_velocity_y = 1;

  // Rectangle Round properties
  data->rectr = GRect(20, 20, 20, 30);
  data->rectr_radius = 5;
  data->rectr_corners_index = GCornersAll;
  data->rectr_velocity_x = 1;
  data->rectr_velocity_y = 1;

  // Circle properties
  data->circle_origin = GPoint(50, 50);
  data->circle_radius = 20;
  data->circle_velocity_x = 1;
  data->circle_velocity_y = 1;
  data->circle_color = s_display_colors[5];

  // Triangle
  data->triangle = gpath_create(&s_triangle_path);
  data->triangle_offset = GPoint(10, 80);
  gpath_move_to(data->triangle, data->triangle_offset);
  data->triangle_velocity_x = 1;
  data->triangle_velocity_y = 1;

  // Open bucket
  data->bucket = gpath_create(&s_bucket_path);
  data->bucket_offset = GPoint(20, 30);
  gpath_move_to(data->bucket, data->bucket_offset);
  data->bucket_velocity_x = 1;
  data->bucket_velocity_y = 1;

  // Other properties
  update_state(data, APP_STATE_FILL_NON_AA);
  data->stroke_width = 1;
  data->moving = true;
  app_timer_register(33, timer_callback, NULL);
}

static void deinit(void) {
  AppData *data = app_state_get_user_data();
  task_free(data);
}

static void s_main(void) {
  init();

  app_event_loop();

  deinit();
}

const PebbleProcessMd* pebble_shapes_get_app_info(void) {
  static const PebbleProcessMdSystem s_app_info = {
    .common.main_func = s_main,
    .name = "Pebble Shapes"
  };
  return (const PebbleProcessMd*) &s_app_info;
}
