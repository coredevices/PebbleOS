/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <applib/preferred_content_size.h>
#include <applib/ui/layer.h>

#define ACTION_BUTTON_RADIUS PBL_IF_ROUND_ELSE(12, 13)

//! How far the action button is pushed past the window edge beyond its radius.
#define ACTION_BUTTON_EDGE_OFFSET                                                                 \
  PREFERRED_CONTENT_SIZE_SWITCH(PreferredContentSizeDefault, /* small */ PBL_IF_ROUND_ELSE(1, 8), \
                                /* medium */ PBL_IF_ROUND_ELSE(1, 8), /* large */ 4,              \
                                /* extralarge */ 4)

//! Width of the action button that is visible inside the window.
static inline int16_t action_button_get_visible_width(void) {
  return ACTION_BUTTON_RADIUS - ACTION_BUTTON_EDGE_OFFSET;
}

//! Action button is the actionable affordance for windows that display actionable content.
//! Action button only provides an update proc instead of entire layer.

//! Draws the action button on the layer with the fill color specified.
//! This expects a layer with a frame and bounds that spans the entire window.
void action_button_draw(GContext *ctx, Layer *layer, GColor fill_color);

//! Update proc of the action button.
//! This expects a layer with a frame and bounds that spans the entire window.
void action_button_update_proc(Layer *action_button_layer, GContext *ctx);
