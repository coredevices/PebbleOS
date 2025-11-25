/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "rocky_api.h"
#include "jerry-api.h"

#include "applib/graphics/text.h"

void rocky_api_graphics_text_init(void);
void rocky_api_graphics_text_deinit(void);
void rocky_api_graphics_text_add_canvas_methods(jerry_value_t obj);
void rocky_api_graphics_text_reset_state(void);

// these structs are exposed here so that we can unit-test the internal state
typedef struct RockyAPISystemFontDefinition {
  const char *js_name;
  const char *res_key;
} RockyAPISystemFontDefinition;

typedef struct RockyAPITextState {
  GFont font;
  const char *font_name;
  GTextOverflowMode overflow_mode;
  GTextAlignment alignment;
  GTextAttributes *text_attributes;
} RockyAPITextState;
