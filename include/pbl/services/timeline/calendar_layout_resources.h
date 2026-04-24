/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gdraw_command_image.h"
#include "applib/graphics/gdraw_command_private.h"
#include "util/attributes.h"
#include "util/size.h"

#define START_ICON_POINTS { { 0, -2 }, { 9, 4 }, { 0, 10 } }

typedef struct PACKED {
  struct {
    GDrawCommandImage image;
  };
  GDrawCommand command;
  GPoint points[STATIC_ARRAY_LENGTH(GPoint, START_ICON_POINTS)];
} CalendarStartIcon;

extern CalendarStartIcon g_calendar_start_icon;

#define END_ICON_POINTS { { 0, 0 }, { 10, 0 }, { 10, 8 }, { 0, 8 } }

typedef struct PACKED {
  struct {
    GDrawCommandImage image;
  };
  GDrawCommand command;
  GPoint points[STATIC_ARRAY_LENGTH(GPoint, END_ICON_POINTS)];
} CalendarEndIcon;

extern CalendarEndIcon g_calendar_end_icon;
