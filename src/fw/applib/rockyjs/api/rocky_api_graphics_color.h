/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "rocky_api.h"

#include "applib/graphics/gtypes.h"
#include "jerry-api.h"

typedef struct RockyAPIGraphicsColorDefinition {
  const char *name;
  const uint8_t value;
} RockyAPIGraphicsColorDefinition;

bool rocky_api_graphics_color_parse(const char *color_value, GColor8 *parsed_color);

bool rocky_api_graphics_color_from_value(jerry_value_t value, GColor *parsed_color);
