/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "content_indicator.h"
#include "content_indicator_private.h"

#include "applib/applib_malloc.auto.h"
#include "applib/app_timer.h"
#include "applib/graphics/gpath.h"
#include "applib/graphics/graphics.h"
#include "kernel/ui/kernel_ui.h"
#include "system/passert.h"
#include "util/buffer.h"
#include "util/size.h"

//! Signature for callbacks provided to prv_content_indicator_iterate()
//! @param content_indicator The current ContentIndicator in the iteration.
//! @param buffer_offset_bytes The offset of the ContentIndicator in the buffer's storage.
//! @param input_context An input context.
//! @param output_context An output context.
//! @return `true` if iteration should continue, `false` otherwise.
typedef bool (*ContentIndicatorIteratorCb)(ContentIndicator *content_indicator,
                                           size_t buffer_offset_bytes,
                                           void *input_context,
                                           void *output_context);

bool prv_content_indicator_init(ContentIndicator *content_indicator) {
  if (!content_indicator) {
    return false;
  }

  *content_indicator = (ContentIndicator){};

  // Add the content indicator to the appropriate buffer
  ContentIndicatorsBuffer *content_indicators_buffer = content_indicator_get_current_buffer();
  Buffer *buffer = &content_indicators_buffer->buffer;
  size_t bytes_written = buffer_add(buffer,
                                    (uint8_t *)&content_indicator,
                                    sizeof(ContentIndicator *));
  // Return whether or not the content indicator was successfully written to the buffer
  return (bytes_written == sizeof(ContentIndicator *));
}

void content_indicator_init(ContentIndicator *content_indicator) {
  bool success = prv_content_indicator_init(content_indicator);
  PBL_ASSERTN(success);
}

//! Returns `true` if `iterator_cb` signaled iteration to end, `false` otherwise.
static bool prv_content_indicator_iterate(ContentIndicatorIteratorCb iterator_cb,
                                          void *input_context,
                                          void *output_context) {
  if (!iterator_cb) {
    return false;
  }

  ContentIndicatorsBuffer *content_indicators_buffer = content_indicator_get_current_buffer();
  Buffer *buffer = &content_indicators_buffer->buffer;
  for (size_t offset = 0; offset < buffer->bytes_written; offset += sizeof(ContentIndicator *)) {
    // We have to break up the access into two parts, otherwise we get a strict-aliasing error
    ContentIndicator **content_indicator_address = (ContentIndicator **)(buffer->data + offset);
    ContentIndicator *content_indicator = *content_indicator_address;
    if (!iterator_cb(content_indicator, offset, input_context, output_context)) {
      return true;
    }
  }

  return false;
}

ContentIndicator *content_indicator_create(void) {
  ContentIndicator *content_indicator = applib_type_zalloc(ContentIndicator);
  if (!content_indicator) {
    return NULL;
  }
  if (!prv_content_indicator_init(content_indicator)) {
    applib_free(content_indicator);
    return NULL;
  }
  return content_indicator;
}

static bool prv_content_indicator_find_for_scroll_layer_cb(ContentIndicator *content_indicator,
                                                           size_t buffer_offset_bytes,
                                                           void *input_context,
                                                           void *output_context) {
  ScrollLayer *target_scroll_layer = input_context;
  if (content_indicator->scroll_layer == target_scroll_layer) {
    *((ContentIndicator **)output_context) = content_indicator;
    return false;
  }
  return true;
}

ContentIndicator *content_indicator_get_for_scroll_layer(ScrollLayer *scroll_layer) {
  if (!scroll_layer) {
    return NULL;
  }

  ContentIndicator *content_indicator = NULL;
  prv_content_indicator_iterate(prv_content_indicator_find_for_scroll_layer_cb,
                                scroll_layer,
                                &content_indicator);
  return content_indicator;
}

ContentIndicator *content_indicator_get_or_create_for_scroll_layer(ScrollLayer *scroll_layer) {
  if (!scroll_layer) {
    return NULL;
  }

  ContentIndicator *content_indicator = content_indicator_get_for_scroll_layer(scroll_layer);
  if (!content_indicator) {
    content_indicator = content_indicator_create();
    if (content_indicator) {
      content_indicator->scroll_layer = scroll_layer;
    }
  }
  return content_indicator;
}

static bool prv_content_indicator_find_buffer_offset_bytes_cb(ContentIndicator *content_indicator,
                                                              size_t buffer_offset_bytes,
                                                              void *input_context,
                                                              void *output_context) {
  ContentIndicator *target_content_indicator = input_context;
  if (content_indicator == target_content_indicator) {
    *((size_t *)output_context) = buffer_offset_bytes;
    return false;
  }
  return true;
}

static void prv_content_indicator_reset_direction(ContentIndicatorDirectionData *direction_data) {
  // Cancel the timeout timer, if necessary
  if (direction_data->timeout_timer) {
    app_timer_cancel(direction_data->timeout_timer);
    direction_data->timeout_timer = NULL;
  }

  ContentIndicatorConfig *config = &direction_data->config;
  if (config->layer) {
    // Set the layer's update proc to be the layer's original update proc
    config->layer->update_proc = direction_data->original_update_proc;
    layer_mark_dirty(config->layer);
  }
}

void content_indicator_deinit(ContentIndicator *content_indicator) {
  if (!content_indicator) {
    return;
  }

  // Deinit the data for each of the directions
  for (size_t i = 0; i < ARRAY_LENGTH(content_indicator->direction_data); i++) {
    ContentIndicatorDirectionData *direction_data = &content_indicator->direction_data[i];
    prv_content_indicator_reset_direction(direction_data);
  }

  // Find the offset of the content indicator in the buffer
  size_t buffer_offset_bytes;
  if (!prv_content_indicator_iterate(prv_content_indicator_find_buffer_offset_bytes_cb,
                                     content_indicator,
                                     &buffer_offset_bytes)) {
    return;
  }

  // Remove the content indicator from the appropriate buffer
  ContentIndicatorsBuffer *content_indicators_buffer = content_indicator_get_current_buffer();
  Buffer *buffer = &content_indicators_buffer->buffer;
  buffer_remove(buffer, buffer_offset_bytes, sizeof(ContentIndicator *));
}

void content_indicator_destroy(ContentIndicator *content_indicator) {
  if (!content_indicator) {
    return;
  }

  content_indicator_deinit(content_indicator);
  applib_free(content_indicator);
}

void content_indicator_destroy_for_scroll_layer(ScrollLayer *scroll_layer) {
  if (!scroll_layer) {
    return;
  }

  ContentIndicator *content_indicator;
  if (prv_content_indicator_iterate(prv_content_indicator_find_for_scroll_layer_cb,
                                    scroll_layer,
                                    &content_indicator)) {
    content_indicator_destroy(content_indicator);
  }
}

bool content_indicator_configure_direction(ContentIndicator *content_indicator,
                                           ContentIndicatorDirection direction,
                                           const ContentIndicatorConfig *config) {
  if (!content_indicator) {
    return false;
  }

  // If NULL is passed for config, reset the data for this direction.
  if (!config) {
    ContentIndicatorDirectionData *direction_data = &content_indicator->direction_data[direction];
    prv_content_indicator_reset_direction(direction_data);
    *direction_data = (ContentIndicatorDirectionData){};
    return true;
  }

  if (!config->layer) {
    return false;
  }

  // Fail if any of the other directions have already been configured with this config's layer
  for (ContentIndicatorDirection dir = 0; dir < NumContentIndicatorDirections; dir++) {
    if (dir == direction) {
      continue;
    }
    ContentIndicatorDirectionData *direction_data = &content_indicator->direction_data[dir];
    if (direction_data->config.layer == config->layer) {
      return false;
    }
  }

  ContentIndicatorDirectionData *direction_data = &content_indicator->direction_data[direction];
  prv_content_indicator_reset_direction(direction_data);
  *direction_data = (ContentIndicatorDirectionData){
    .direction = direction,
    .config = *config,
    .original_update_proc = config->layer->update_proc,
  };

  return true;
}

static bool prv_content_indicator_find_direction_data_cb(ContentIndicator *content_indicator,
                                                         size_t buffer_offset_bytes,
                                                         void *input_context,
                                                         void *output_context) {
  Layer *target_layer = input_context;
  for (ContentIndicatorDirection dir = 0; dir < NumContentIndicatorDirections; dir++) {
    ContentIndicatorDirectionData *direction_data = &content_indicator->direction_data[dir];
    if (direction_data->config.layer == target_layer) {
      *((ContentIndicatorDirectionData **)output_context) = direction_data;
      return false;
    }
  }
  return true;
}

void content_indicator_draw_arrow(GContext *ctx,
                                  const GRect *frame,
                                  ContentIndicatorDirection direction,
                                  GColor fg_color,
                                  GColor bg_color,
                                  GAlign alignment) {
  // Fill the background color
  graphics_context_set_fill_color(ctx, bg_color);
  graphics_fill_rect(ctx, frame);

  // Pick the arrow to draw
  const int16_t arrow_height = 6;
  const GPathInfo arrow_up_path_info = {
    .num_points = 3,
    .points = (GPoint[]) {{0, arrow_height}, {(arrow_height + 1), 0},
                          {((arrow_height * 2) + 1), arrow_height}}
  };
  const GPathInfo arrow_down_path_info = {
    .num_points = 3,
    .points = (GPoint[]) {{0, 0}, {(arrow_height + 1), arrow_height},
                          {((arrow_height * 2) + 1), 0}}
  };
  const GPathInfo *arrow_path_info;
  switch (direction) {
    case ContentIndicatorDirectionUp:
      arrow_path_info = &arrow_up_path_info;
      break;
    case ContentIndicatorDirectionDown:
      arrow_path_info = &arrow_down_path_info;
      break;
    default:
      WTF;
  }

  // Draw the arrow
  GPath arrow_path;
  gpath_init(&arrow_path, arrow_path_info);
  // Align the arrow within the provided bounds
  GRect arrow_box = gpath_outer_rect(&arrow_path);
  grect_align(&arrow_box, frame, alignment, true /* clip */);
  gpath_move_to(&arrow_path, arrow_box.origin);

  const bool prev_antialiased = graphics_context_get_antialiased(ctx);
  graphics_context_set_antialiased(ctx, false);
  graphics_context_set_fill_color(ctx, fg_color);
  gpath_draw_filled(ctx, &arrow_path);
  graphics_context_set_antialiased(ctx, prev_antialiased);
}

T_STATIC void prv_content_indicator_update_proc(Layer *layer, GContext *ctx) {
  // Find the direction data corresponding to the layer that should be updated
  ContentIndicatorDirectionData *direction_data;
  if (!prv_content_indicator_iterate(prv_content_indicator_find_direction_data_cb,
                                     layer,
                                     &direction_data)) {
    return;
  }

  ContentIndicatorConfig *config = &direction_data->config;
  content_indicator_draw_arrow(ctx,
                               &layer->bounds,
                               direction_data->direction,
                               config->colors.foreground,
                               config->colors.background,
                               config->alignment);
}

bool content_indicator_get_content_available(ContentIndicator *content_indicator,
                                             ContentIndicatorDirection direction) {
  if (!content_indicator) {
    return false;
  }

  ContentIndicatorDirectionData *direction_data = &content_indicator->direction_data[direction];
  return direction_data->content_available;
}


void content_indicator_set_content_available(ContentIndicator *content_indicator,
                                             ContentIndicatorDirection direction,
                                             bool available) {
  if (!content_indicator) {
    return;
  }

  ContentIndicatorDirectionData *direction_data = &content_indicator->direction_data[direction];
  direction_data->content_available = available;

  ContentIndicatorConfig *config = &direction_data->config;

  if (!config->layer) {
    return;
  }

  // Cleans potentially scheduled timer, resets update_proc, marks dirty
  prv_content_indicator_reset_direction(direction_data);

  if (available) {
    // Set the layer's update proc to be the arrow-drawing update proc and mark it as dirty
    config->layer->update_proc = prv_content_indicator_update_proc;
    layer_mark_dirty(config->layer);
    // If the arrow should time out and a timer isn't already scheduled, register a timeout timer
    if (config->times_out && !direction_data->timeout_timer) {
      direction_data->timeout_timer = app_timer_register(
        CONTENT_INDICATOR_TIMEOUT_MS,
        (AppTimerCallback)prv_content_indicator_reset_direction,
        direction_data);
    }
  }
}

void content_indicator_init_buffer(ContentIndicatorsBuffer *content_indicators_buffer) {
  if (!content_indicators_buffer) {
    return;
  }
  buffer_init(&content_indicators_buffer->buffer, CONTENT_INDICATOR_BUFFER_SIZE_BYTES);
}
