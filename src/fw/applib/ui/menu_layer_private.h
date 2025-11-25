/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "menu_layer.h"

struct MenuIterator;

typedef void (*MenuIteratorCallback)(struct MenuIterator *it);

typedef struct MenuIterator {
  MenuLayer * menu_layer;
  MenuCellSpan cursor;
  int16_t cell_bottom_y;
  MenuIteratorCallback row_callback_before_geometry;
  MenuIteratorCallback row_callback_after_geometry;
  MenuIteratorCallback section_callback;
  bool should_continue; // callback can set this to false if the row-loop should be exited.
} MenuIterator;

typedef struct MenuRenderIterator {
  MenuIterator it;
  GContext* ctx;
  int16_t content_top_y;
  int16_t content_bottom_y;
  bool cache_set:1;
  bool cursor_in_frame:1;
  MenuCellSpan new_cache;
  Layer cell_layer;
} MenuRenderIterator;
