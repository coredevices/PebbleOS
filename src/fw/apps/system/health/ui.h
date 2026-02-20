/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/ui.h"

void health_ui_draw_text_in_box(GContext *ctx, const char *text, const GRect drawing_bounds,
                                const int16_t y_offset, const GFont small_font, GColor box_color,
                                GColor text_color);

void health_ui_render_typical_text_box(GContext *ctx, Layer *layer, const char *value_text);
