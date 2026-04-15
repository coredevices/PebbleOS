/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "item.h"
#include "layout_layer.h"
#include "timeline_layout.h"

#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"

typedef struct {
  TimelineLayout timeline_layout;
} AlarmLayout;

LayoutLayer *alarm_layout_create(const LayoutLayerConfig *config);

bool alarm_layout_verify(bool existing_attributes[]);
