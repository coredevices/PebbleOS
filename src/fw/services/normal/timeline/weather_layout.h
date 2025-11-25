/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "item.h"
#include "layout_layer.h"
#include "timeline_layout.h"

#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "apps/system_apps/timeline/text_node.h"

typedef enum {
  WeatherTimeType_None = 0,
  WeatherTimeType_Pin,
} WeatherTimeType;

typedef struct {
  TimelineLayout timeline_layout;
} WeatherLayout;

LayoutLayer *weather_layout_create(const LayoutLayerConfig *config);

bool weather_layout_verify(bool existing_attributes[]);
