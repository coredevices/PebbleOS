/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "item.h"
#include "layout_layer.h"
#include "timeline_layout.h"

#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "applib/ui/bitmap_layer.h"

typedef struct {
  TimelineLayout timeline_layout;
} GenericLayout;

LayoutLayer *generic_layout_create(const LayoutLayerConfig *config);

bool generic_layout_verify(bool existing_attributes[]);
