/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "layer.h"
#include "picker_touch.h"

#include "applib/graphics/gtypes.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*ValuePickerFormatter)(char *buffer, size_t buffer_size, int32_t value);

typedef struct ValuePickerStyle {
  GColor background_color;
  GColor accent_color;
  GColor value_color;
  GColor metadata_color;
  GColor neighbor_color;
  const char *value_font_key;
  const char *unit_font_key;
  const char *neighbor_font_key;
  int16_t title_y;
} ValuePickerStyle;

typedef struct ValuePickerContent {
  const char *title;
  const char *unit;
  int32_t value;
  int32_t min_value;
  int32_t max_value;
  int32_t step;
  //! NULL formats the value as a plain integer.
  ValuePickerFormatter format;
} ValuePickerContent;

struct ValuePickerLayer;

typedef void (*ValuePickerLayerChangedHandler)(struct ValuePickerLayer *picker_layer, int direction,
                                               void *context);
typedef bool (*ValuePickerLayerCanStepHandler)(struct ValuePickerLayer *picker_layer, int direction,
                                               void *context);

typedef struct ValuePickerLayerCallbacks {
  ValuePickerLayerChangedHandler changed;
  ValuePickerLayerCanStepHandler can_step;
} ValuePickerLayerCallbacks;

typedef struct ValuePickerLayer {
  Layer layer;
  ValuePickerContent content;
  ValuePickerStyle style;
  ValuePickerLayerCallbacks callbacks;
  void *callback_context;
  PickerTouch touch;
  bool touch_enabled;
} ValuePickerLayer;

const ValuePickerStyle *value_picker_layer_default_style(void);

void value_picker_layer_init(ValuePickerLayer *picker_layer, const GRect *frame,
                             const ValuePickerContent *content, const ValuePickerStyle *style,
                             ValuePickerLayerCallbacks callbacks, void *callback_context);

void value_picker_layer_deinit(ValuePickerLayer *picker_layer);

void value_picker_layer_enable_touch(ValuePickerLayer *picker_layer);

void value_picker_layer_disable_touch(ValuePickerLayer *picker_layer);

bool value_picker_layer_step(ValuePickerLayer *picker_layer, int direction);

void value_picker_layer_set_content(ValuePickerLayer *picker_layer,
                                    const ValuePickerContent *content);

int32_t value_picker_layer_get_value(const ValuePickerLayer *picker_layer);
