/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"

typedef enum {
  TimelinePinsDemo_Default = 0,
  TimelinePinsDemo_Notifications,
  TimelinePinsDemo_OneDayAway,
  TimelinePinsDemo_OngoingEvent,
  TimelinePinsDemo_TodayAndTomorrow,
  TimelinePinsDemoCount,
} TimelinePinsDemoSet;

void timeline_pins_demo_add_pins(TimelinePinsDemoSet pin_set);

const PebbleProcessMd *timeline_pins_get_app_info(void);
