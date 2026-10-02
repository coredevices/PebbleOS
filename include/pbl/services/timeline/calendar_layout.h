/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "layout_layer.h"
#include "timeline_layout.h"

#include "applib/ui/ui.h"

/**
 * @defgroup services_timeline_calendar_layout Calendar layout
 * @ingroup services_timeline
 * @brief Layout of calendar pins (LayoutIdCalendar).
 * @{
 */

/** @brief Recurrence of a calendar event, the value of AttributeIdDisplayRecurring. */
typedef enum {
  /** Not recurring. */
  CalendarRecurringTypeNone = 0,
  /** Recurring; the card says so. */
  CalendarRecurringTypeRecurring,
} CalendarRecurringType;

/** @brief Calendar pin layout. */
typedef struct {
  /** Base timeline layout. */
  TimelineLayout timeline_layout;
  /** Date text layer. */
  TextLayer date_layer;
  /** Buffer of the day and date text. */
  char day_date_buffer[TIME_STRING_DAY_DATE_LENGTH];
} CalendarLayout;

/**
 * @brief Create a calendar layout.
 *
 * @param config Configuration; its context must be a TimelineLayoutInfo.
 * @return New layout, allocated on the calling task's heap.
 */
LayoutLayer *calendar_layout_create(const LayoutLayerConfig *config);

/**
 * @brief Check the attributes of a calendar pin.
 *
 * @param existing_attributes Array of NumAttributeIds flags, indexed by AttributeId.
 * @return true if a title is present.
 */
bool calendar_layout_verify(bool existing_attributes[]);

/** @} */
