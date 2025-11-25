/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "status_bar_layer.h"

#include "animation_timing.h"
#include "applib/app_logging.h"
#include "applib/applib_malloc.auto.h"
#include "applib/fonts/fonts.h"
#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/graphics_line.h"
#include "applib/graphics/text.h"
#include "applib/ui/window_stack.h"
#include "kernel/ui/kernel_ui.h"
#include "process_state/app_state/app_state.h"
#include "services/common/clock.h"
#include "syscall/syscall.h"
#include "syscall/syscall_internal.h"
#include "system/passert.h"
#include "util/math.h"
#include "util/string.h"

typedef struct StatusBarTextFormat {
  GTextOverflowMode overflow_mode;
  GTextAlignment text_alignment;
  GFont font;
} StatusBarTextFormat;

static ALWAYS_INLINE StatusBarTextFormat prv_get_text_format(void) {
  const PlatformType platform = process_manager_current_platform();
  const char *font_key = PBL_PLATFORM_SWITCH(platform,
      /*aplite*/ FONT_KEY_GOTHIC_14,
      /*basalt*/ FONT_KEY_GOTHIC_14,
      /*chalk*/ FONT_KEY_GOTHIC_14,
      /*diorite*/ FONT_KEY_GOTHIC_14,
      /*emery*/ FONT_KEY_GOTHIC_18,
      /*flint*/ FONT_KEY_GOTHIC_14);
  return (StatusBarTextFormat) {
    .overflow_mode = GTextOverflowModeTrailingEllipsis,
    .text_alignment = GTextAlignmentCenter,
    .font = fonts_get_system_font(font_key),
  };
}

static int prv_height(void) {
  const PlatformType platform = process_manager_current_platform();
  return _STATUS_BAR_LAYER_HEIGHT(platform);
}

// Function prototypes
static void prv_status_bar_layer_update_clock(StatusBarLayer *status_bar_layer);
static void prv_tick_timer_handler_cb(PebbleEvent *e, void *cb_data);

static void prv_status_bar_property_changed(struct Layer *layer) {
  const int16_t height = prv_height();
  if (layer->frame.size.h != height) {
    layer->frame.size.h = height;
  }
  if (layer->bounds.size.h != height) {
    layer->bounds.size.h = height;
  }
}

static void prv_status_bar_layer_render(Layer *layer, GContext *ctx) {
  StatusBarLayer *status_bar_layer = (StatusBarLayer *)layer;
  // During a window transition with fixed status bars, ignore horizontal offset of the window.
  // For two windows with a status bar at (0,0), this will make sure that both status bars share the
  // same screen coordinates despite the window movement - the clip_box prevents overdrawing.
  // This is a first step towards a general purpose system for static status bars.

  const int16_t stored_drawing_box_x = ctx->draw_state.drawing_box.origin.x;
  if (window_stack_is_animating_with_fixed_status_bar(app_state_get_window_stack())) {
    ctx->draw_state.drawing_box.origin.x -= status_bar_layer->layer.window->layer.frame.origin.x;
  }

  status_bar_layer_render(ctx, &status_bar_layer->layer.bounds, &status_bar_layer->config);

  ctx->draw_state.drawing_box.origin.x = stored_drawing_box_x;
}

void status_bar_layer_init(StatusBarLayer *status_bar_layer) {
  PBL_ASSERTN(status_bar_layer);
  *status_bar_layer = (StatusBarLayer){
    .previous_min_of_day = -1,
  };

  // The status bar needs to be as wide as the framebuffer we will render it into, which may be less
  // wide than the display e.g. if an app is running in bezel mode. The current graphics context's
  // contains the appropriate size.
  GContext *ctx = graphics_context_get_current_context();
  const GSize current_framebuffer_size = graphics_context_get_framebuffer_size(ctx);

  layer_init(&status_bar_layer->layer, &GRect(0, 0, current_framebuffer_size.w, prv_height()));
  status_bar_layer->layer.update_proc = prv_status_bar_layer_render;
  status_bar_layer->layer.property_changed_proc = prv_status_bar_property_changed;

  // tick event to callback which checks every second if the time is correct
  status_bar_layer->tick_event = (EventServiceInfo){
    .type = PEBBLE_TICK_EVENT,
    .handler = prv_tick_timer_handler_cb,
    .context = status_bar_layer};
  event_service_client_subscribe(&(status_bar_layer->tick_event));

  status_bar_layer->config = (StatusBarLayerConfig){
    .foreground_color = GColorWhite,
    .background_color = GColorBlack,
    .separator.mode = StatusBarLayerSeparatorModeNone,
  };

  status_bar_layer->title_timer_id = TIMER_INVALID_ID;
}

StatusBarLayer *status_bar_layer_create(void) {
  StatusBarLayer *layer = applib_type_zalloc(StatusBarLayer);
  if (layer) {
    status_bar_layer_init(layer);
  }
  return layer;
}

void status_bar_layer_destroy(StatusBarLayer *status_bar_layer) {
  if (!status_bar_layer) {
    return;
  }
  status_bar_layer_deinit(status_bar_layer);
  applib_free(status_bar_layer);
}

void status_bar_layer_deinit(StatusBarLayer *status_bar_layer) {
  layer_deinit(&status_bar_layer->layer);
  if (status_bar_layer->title_timer_id != TIMER_INVALID_ID) {
    app_timer_cancel(status_bar_layer->title_timer_id);
  }
  event_service_client_unsubscribe(&(status_bar_layer->tick_event));
}

Layer *status_bar_layer_get_layer(StatusBarLayer *status_bar_layer) {
  PBL_ASSERTN(status_bar_layer);
  return &status_bar_layer->layer;
}

void status_bar_layer_set_colors(StatusBarLayer *status_bar_layer, GColor background,
                                 GColor foreground) {
  PBL_ASSERTN(status_bar_layer);

  if (gcolor_equal(status_bar_layer->config.background_color, background) &&
      gcolor_equal(status_bar_layer->config.foreground_color, foreground)) {
    return;
  }

  status_bar_layer->config.background_color = background;
  status_bar_layer->config.foreground_color = foreground;
  layer_mark_dirty(&(status_bar_layer->layer));
}

GColor status_bar_layer_get_background_color(const StatusBarLayer *status_bar_layer) {
  PBL_ASSERTN(status_bar_layer);
  return status_bar_layer->config.background_color;
}

GColor status_bar_layer_get_foreground_color(const StatusBarLayer *status_bar_layer) {
  PBL_ASSERTN(status_bar_layer);
  return status_bar_layer->config.foreground_color;
}

void status_bar_layer_set_title(StatusBarLayer *status_bar_layer,
                                const char *text,
                                bool revert,
                                bool animated) {
  // copies the contents at text into title_text_buffer for display
  strncpy(status_bar_layer->config.title_text_buffer, text, TITLE_TEXT_BUFFER_SIZE);
  if (revert) { // revert title text back to clock time after STATUS_BAR_LAYER_TITLE_TIMEOUT
    if (status_bar_layer->title_timer_id != TIMER_INVALID_ID) {
        app_timer_cancel(status_bar_layer->title_timer_id);
    }
    status_bar_layer->title_timer_id = app_timer_register(STATUS_BAR_LAYER_TITLE_TIMEOUT,
                                                      status_bar_layer_reset_title,
                                                      status_bar_layer);
  }
  status_bar_layer->config.mode = StatusBarLayerModeLoading;
  layer_mark_dirty(&(status_bar_layer->layer));
}

const char *status_bar_layer_get_title(const StatusBarLayer *status_bar_layer) {
  PBL_ASSERTN(status_bar_layer);
  return status_bar_layer->config.title_text_buffer;
}

void status_bar_layer_reset_title(void *cb_data) {
  StatusBarLayer *status_bar_layer = (StatusBarLayer *)cb_data;
  // set title text mode to 'clock', update text, and setup timer to allow clock text to update
  status_bar_layer->config.mode = StatusBarLayerModeClock;
  prv_status_bar_layer_update_clock(status_bar_layer);
}

void status_bar_layer_set_info_text(StatusBarLayer *status_bar_layer, const char *text) {
  PBL_ASSERTN(status_bar_layer);
  strncpy(status_bar_layer->config.info_text_buffer, text, INFO_TEXT_BUFFER_SIZE);
  layer_mark_dirty(&(status_bar_layer->layer));
}

// Sets info text either ot X/Y or percentage if total is larger than MAX_INFO_TOTAL
void status_bar_layer_set_info_progress(StatusBarLayer *status_bar_layer,
                                        uint16_t current,
                                        uint16_t total) {
  PBL_ASSERTN(status_bar_layer);
  if (current > total) {
    // do not display
    return;
  } else {
    char *str = status_bar_layer->config.info_text_buffer;
    memset(str, 0 , INFO_TEXT_BUFFER_SIZE);
    if (total > MAX_INFO_TOTAL) { // total is large; display as a percentage
      itoa_int(current * 100 / total, str, 10);
      strcat(str, "%");
    } else { // display as an X/Y
      itoa_int(current, str, 10);
      strcat(str, "/");
      char buffer[(INFO_TEXT_BUFFER_SIZE - 1) / 2];
      itoa_int(total, buffer, 10);
      strcat(str, buffer);
    }
  }
  layer_mark_dirty(&(status_bar_layer->layer));
}

const char *status_bar_layer_get_info_text(const StatusBarLayer *status_bar_layer) {
  PBL_ASSERTN(status_bar_layer);
  return status_bar_layer->config.info_text_buffer;
}

void status_bar_layer_reset_info(StatusBarLayer *status_bar_layer) {
  PBL_ASSERTN(status_bar_layer);
  memset(status_bar_layer->config.info_text_buffer, 0,
         sizeof(status_bar_layer->config.info_text_buffer));
  layer_mark_dirty(&status_bar_layer->layer);
}

void status_bar_layer_set_separator_mode(StatusBarLayer *status_bar_layer,
                                         StatusBarLayerSeparatorMode mode) {
  PBL_ASSERTN(status_bar_layer);
  status_bar_layer->config.separator.mode = mode;
  layer_mark_dirty(&(status_bar_layer->layer));
}

void status_bar_layer_set_separator_load_percentage(StatusBarLayer *status_bar_layer,
                                                int16_t percentage) {
  PBL_ASSERTN(status_bar_layer);
  // TODO: animation related function
  layer_mark_dirty(&(status_bar_layer->layer));
}

StatusBarLayerSeparatorMode status_bar_layer_get_separator_mode(
    const StatusBarLayer *status_bar_layer) {
  PBL_ASSERTN(status_bar_layer);
  return status_bar_layer->config.separator.mode;
}

//*****************************************************************************
//* INTERNAL FUNCTIONS
//*****************************************************************************

// Manual internal function to refresh title text clock and mark dirty
static void prv_status_bar_layer_update_clock(StatusBarLayer *status_bar_layer) {
  clock_copy_time_string(status_bar_layer->config.title_text_buffer,
                         sizeof(status_bar_layer->config.title_text_buffer));
  layer_mark_dirty(&(status_bar_layer->layer));
}

// Callback for updating title text clock's time
static void prv_tick_timer_handler_cb(PebbleEvent *e, void *cb_data) {
  StatusBarLayer *status_bar_layer = (StatusBarLayer *)cb_data;
  if (status_bar_layer->config.mode != StatusBarLayerModeClock) {
    return;
  }
  struct tm currtime;
  sys_localtime_r(&e->clock_tick.tick_time, &currtime);
  const int min_of_day = (currtime.tm_hour * 60) + currtime.tm_min;
  if (status_bar_layer->previous_min_of_day != min_of_day) {
    prv_status_bar_layer_update_clock(status_bar_layer); // update clock text and mark dirty
    status_bar_layer->previous_min_of_day = min_of_day;
  }
}

// Calculate position and renders text
static void prv_status_bar_layer_render_text(GContext *ctx,
                                             int16_t min_x,
                                             int16_t max_x,
                                             int16_t min_y,
                                             int16_t max_y,
                                             char *data) {
  const StatusBarTextFormat text_format = prv_get_text_format();
  const GFont font = text_format.font;
  const uint8_t font_height = fonts_get_font_height(font);
  const int16_t center = (max_x + min_x) / 2;
  const int16_t left_width = center - min_x;
  const int16_t right_width = max_x - center;
  // Use larger distance from the center to min_x or max_x as half of the width (odd num pixels)
  const int16_t width = 2 * MAX(left_width, right_width);
  // starting point of text needs to be half width left of the center.
  const int16_t x_start = center - width / 2;
  // Position the text vertically offset from the bottom of the status bar
  const int16_t y = min_y + max_y - (2 * STATUS_BAR_LAYER_SEPARATOR_Y_OFFSET) - font_height;
  graphics_draw_text(ctx,
                     data,
                     font,
                     GRect(x_start, y, width, font_height),
                     text_format.overflow_mode,
                     text_format.text_alignment, NULL);
}

// Renders all of StatusBarLayer when layer_mark_dirty triggers LayerUpdateProc
void status_bar_layer_render(GContext *ctx, const GRect *bounds, StatusBarLayerConfig *config) {
  // define x and y coords of status_bar_layer
  int16_t x_offset_l = bounds->origin.x;
  int16_t x_offset_r = x_offset_l + bounds->size.w;
  int16_t y_offset_top = bounds->origin.y;
  int16_t y_offset_bottom = y_offset_top + bounds->size.h;
  //  TODO: x offset for ICON_MARGINs
  //  for (uint8_t i = 0; i < ARRAY_LENGTH(s_left_icons); ++i) {
  //    x_offset_l += s_left_icons[i](ctx, x_offset_l, GAlignLeft);
  //  }

  // Fill background of status layer using bounds, if color is not transparent
  // TODO: use of gcolor_is_transparent to determine whether or
  if (!gcolor_is_transparent(config->background_color)) {
    graphics_context_set_fill_color(ctx, config->background_color);
    graphics_fill_rect(ctx, bounds);
  }

  // Set context text color and compositing mode
  graphics_context_set_text_color(ctx, config->foreground_color);
  graphics_context_set_compositing_mode(ctx, GCompOpSet);

  // update title buffer with time if in StatusBarLayerModeClock
  if (config->mode == StatusBarLayerModeClock) {
    clock_copy_time_string(config->title_text_buffer,
                           sizeof(config->title_text_buffer));
  }

  if (config->mode != StatusBarLayerModeCustomText) { // draw center text
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
    prv_status_bar_layer_render_text(ctx, x_offset_l, x_offset_r, y_offset_top, y_offset_bottom,
                                     config->title_text_buffer);
  } else { // TODO: here goes center text animations
  }

  // render info text
  GFont info_font = prv_get_text_format().font;
  // find width of info text
  GSize max_used_size = graphics_text_layout_get_max_used_size(ctx, config->info_text_buffer,
                                         info_font,
                                         GRect(0, 0, 100, prv_height()),
                                         GTextOverflowModeTrailingEllipsis,
                                         GTextAlignmentCenter,
                                         NULL);
  // use the width found to render the info text
  int16_t info_text_left_offset = (int16_t) (x_offset_r - max_used_size.w -
                                    STATUS_BAR_LAYER_INFO_PADDING);
  int16_t info_text_right_offset = (int16_t) (x_offset_r - STATUS_BAR_LAYER_INFO_PADDING);
  prv_status_bar_layer_render_text(ctx,
                                   info_text_left_offset,
                                   info_text_right_offset,
                                   y_offset_top,
                                   y_offset_bottom,
                                   config->info_text_buffer);

  // draw the separator
  if (config->separator.mode != StatusBarLayerSeparatorModeNone) {
    graphics_context_set_stroke_color(ctx, config->foreground_color);
    GPoint origin = {x_offset_l, (int16_t) (y_offset_bottom - STATUS_BAR_LAYER_SEPARATOR_Y_OFFSET)};
    graphics_draw_horizontal_line_dotted(ctx, origin, (uint16_t) x_offset_r);
  }
}

bool layer_is_status_bar_layer(Layer *layer) {
  return layer && layer->update_proc == prv_status_bar_layer_render;
}

int16_t status_layer_get_title_text_width(StatusBarLayer *status_bar_layer) {
  // other modes not supported
  PBL_ASSERTN(status_bar_layer->config.mode == StatusBarLayerModeClock);

  const StatusBarTextFormat text_format = prv_get_text_format();
  char time_text_buffer[TITLE_TEXT_BUFFER_SIZE];
  clock_copy_time_string(time_text_buffer, sizeof(time_text_buffer));
  GContext *ctx = graphics_context_get_current_context();
  return graphics_text_layout_get_max_used_size(ctx,
                                                time_text_buffer,
                                                text_format.font,
                                                status_bar_layer->layer.bounds,
                                                text_format.overflow_mode,
                                                text_format.text_alignment,
                                                NULL).w;
}
