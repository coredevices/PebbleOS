/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "item.h"
#include "layout_layer.h"
#include "timeline_layout.h"

#include "applib/graphics/gdraw_command_image.h"
#include "applib/graphics/graphics.h"
#include "applib/graphics/text.h"
#include "applib/ui/ui.h"

typedef enum {
  CalendarRecurringTypeNone = 0,
  CalendarRecurringTypeRecurring,
} CalendarRecurringType;

typedef struct {
  TimelineLayout timeline_layout;
  TextLayer date_layer;
  char day_date_buffer[TIME_STRING_DAY_DATE_LENGTH];
} CalendarLayout;

LayoutLayer *calendar_layout_create(const LayoutLayerConfig *config);

bool calendar_layout_verify(bool existing_attributes[]);
