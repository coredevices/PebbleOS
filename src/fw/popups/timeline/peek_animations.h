/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"

#define PEEK_ANIMATIONS_SPEED_LINES_OFFSET_X (7)

void peek_animations_draw_compositor_foreground_speed_lines(GContext *ctx, GPoint offset);

void peek_animations_draw_compositor_background_speed_lines(GContext *ctx, GPoint offset);

void peek_animations_draw_timeline_speed_lines(GContext *ctx, GPoint offset);
