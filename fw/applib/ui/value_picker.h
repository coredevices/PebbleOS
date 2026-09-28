/* SPDX-FileCopyrightText: 2026 Aliaksandr Karnilovich */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "layer.h"

#include "applib/graphics/gtypes.h"

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

//! Draws the title, the boxed current value with its unit, and the neighboring values: the next
//! higher value above and the next lower value below.
void value_picker_draw(GContext *ctx, const GRect *bounds, const ValuePickerContent *content,
                       const ValuePickerStyle *style);

const ValuePickerStyle *value_picker_default_style(void);

//! Converts a vertical drag into value steps, rounded to the nearest step. Dragging down is
//! positive, pulling the higher value above into the box.
int16_t value_picker_steps_from_drag(int16_t delta_y);

//! @param direction +1 to increase the value, -1 to decrease it.
typedef void (*ValuePickerStepHandler)(int direction, void *context);

//! Touch input for a value picker window: a vertical drag steps the value and a horizontal swipe
//! navigates (right = BACK, left = SELECT). Taps on the action bar reach it through the button
//! bridge.
typedef struct ValuePickerTouch {
  //! Touch target covering the parent. The window root layer cannot be one: touch routing never
  //! resolves to it.
  Layer layer;
  ValuePickerStepHandler step;
  void *context;
  int16_t applied_steps;
  //! Layout-compatible with TouchNavWidgetNode; declared unconditionally so the struct size is
  //! board-independent.
  struct {
    void *next;
    void *layer;
    void *ops;
    void *widget;
  } touch_nav_node;
} ValuePickerTouch;

//! Adds the touch layer as a child of `parent`. Call before adding the action bar so the action
//! bar stays on top of it.
void value_picker_touch_init(ValuePickerTouch *touch, Layer *parent, ValuePickerStepHandler step,
                             void *context);

void value_picker_touch_deinit(ValuePickerTouch *touch);
