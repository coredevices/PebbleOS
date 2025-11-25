/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "applib/graphics/gtypes.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/layer.h"

#define MIN_PROGRESS_PERCENT 0
#define MAX_PROGRESS_PERCENT 100

#define PROGRESS_SUGGESTED_HEIGHT PBL_IF_COLOR_ELSE(6, 7)
#define PROGRESS_SUGGESTED_CORNER_RADIUS PBL_IF_COLOR_ELSE(2, 3)

//! Note: Do NOT modify the first two elements of this struct since type punning
//! is used to grab the progress_percent during the layer's update_proc
typedef struct {
  Layer layer;
  unsigned int progress_percent;
  GColor foreground_color;
  GColor background_color;
  int16_t corner_radius;
} ProgressLayer;

//! Draw a progress bar inside the given frame
//!
//! Note: the frame *must* be at least 8 pixels wide and 8 pixels tall.
//! This is because 2 pixels of white padding are placed around the progress
//! bar, and the progress bar itself is bounded by a 2 pixel black rounded rect.
//! For greatest sex appeal, make the progress bar larger than 8x8.
void progress_layer_init(ProgressLayer* progress_layer, const GRect *frame);

void progress_layer_deinit(ProgressLayer* progress_layer);

void progress_layer_set_foreground_color(ProgressLayer* progress_layer, GColor color);

void progress_layer_set_background_color(ProgressLayer* progress_layer, GColor color);
//! Convenience function to set the progress layer's progress and mark the
//! layer dirty.
void progress_layer_set_progress(ProgressLayer* progress_layer, unsigned int progress_percent);

void progress_layer_set_corner_radius(ProgressLayer* progress_layer, uint16_t corner_radius);
