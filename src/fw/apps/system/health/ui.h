/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/ui.h"

void health_ui_draw_text_in_box(GContext *ctx, const char *text, const GRect drawing_bounds,
                                const int16_t y_offset, const GFont small_font, GColor box_color,
                                GColor text_color);

//! Render the "TYPICAL <day>" pill with a single value line below the header.
//! Used on narrow displays and on the sleep card.
//!
//! @param value_text Pre-formatted value text for the lower line (e.g. the
//!   daily-total step count, or the typical sleep duration).
void health_ui_render_typical_text_box(GContext *ctx, Layer *layer,
                                       const char *value_text);

//! Render the "TYPICAL <day>" pill as a two-column split: each column shows a
//! value on top and a label beneath, separated by a vertical divider. Used on
//! wide (>= 200px) displays for the activity card. The caller supplies all four
//! strings so the renderer stays generic (not steps-specific).
//!
//! @param left_value  Pre-formatted value for the left column (top).
//! @param left_label  Label for the left column (bottom).
//! @param right_value Pre-formatted value for the right column (top).
//! @param right_label Label for the right column (bottom).
void health_ui_render_split_typical_text_box(GContext *ctx, Layer *layer,
                                             const char *left_value, const char *left_label,
                                             const char *right_value, const char *right_label);
