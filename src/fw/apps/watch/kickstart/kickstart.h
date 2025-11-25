/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/ui.h"
#include "process_management/pebble_process_md.h"

typedef struct KickstartData {
  Window window;
  Layer base_layer;

  int32_t current_steps;
  int32_t typical_steps;
  int32_t daily_steps_avg;
  int32_t current_bpm;

#if PBL_BW
  GBitmap shoe;
#else
  GBitmap shoe_blue;
  GBitmap shoe_green;
#endif
#if PBL_COLOR && PBL_DISPLAY_WIDTH == 144 && PBL_DISPLAY_HEIGHT == 168
  GBitmap shoe_blue_small;
  GBitmap shoe_green_small;
#endif
  GBitmap heart_icon;

  GFont steps_font;
  GFont time_font;
  GFont am_pm_font;

  bool screen_is_obstructed;
  char steps_buffer[8];
} KickstartData;

const PebbleProcessMd* kickstart_get_app_info();
