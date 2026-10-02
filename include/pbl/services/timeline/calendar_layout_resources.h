/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gdraw_command_image.h"
#include "applib/graphics/gdraw_command_private.h"
#include "pbl/kernel/compiler.h"
#include "pbl/util/size.h"

/**
 * @defgroup services_timeline_calendar_layout_resources Calendar layout icons
 * @ingroup services_timeline
 * @brief Vector icons next to the start and end times on calendar cards.
 * @{
 */

/** @brief Points of the start icon, a triangle. */
#define START_ICON_POINTS {{0, -2}, {9, 4}, {0, 10}}

/** @brief Draw command image of the start icon with its single path inline. */
typedef struct PBL_PACKED {
  /** Image header. */
  struct {
    /** Image. */
    GDrawCommandImage image;
  };
  /** Path command. */
  GDrawCommand command;
  /** Path points. */
  GPoint points[STATIC_ARRAY_LENGTH(GPoint, START_ICON_POINTS)];
} CalendarStartIcon;

/** @brief Icon next to the start time. */
extern CalendarStartIcon g_calendar_start_icon;

/** @brief Points of the end icon, a rectangle. */
#define END_ICON_POINTS {{0, 0}, {10, 0}, {10, 8}, {0, 8}}

/** @brief Draw command image of the end icon with its single path inline. */
typedef struct PBL_PACKED {
  /** Image header. */
  struct {
    /** Image. */
    GDrawCommandImage image;
  };
  /** Path command. */
  GDrawCommand command;
  /** Path points. */
  GPoint points[STATIC_ARRAY_LENGTH(GPoint, END_ICON_POINTS)];
} CalendarEndIcon;

/** @brief Icon next to the end time. */
extern CalendarEndIcon g_calendar_end_icon;

/** @} */
