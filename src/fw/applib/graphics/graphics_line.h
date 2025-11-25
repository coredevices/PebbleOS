/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "gtypes.h"

//! @addtogroup Graphics
//! @{

//!   @addtogroup Drawing Drawing Primitives
//!   @{

//! Draws line in the current stroke color, current stroke width and AA flag
//! @param ctx The destination graphics context in which to draw
//! @param p0 The starting point of the line
//! @param p1 The ending point of the line
void graphics_draw_line(GContext* ctx, GPoint p0, GPoint p1);

//!   @} // end addtogroup Drawing
//! @} // end addtogroup Graphics

//! @internal
//! Draws non-antialiased 1px width line between given points, will adjust to drawing_box
MOCKABLE void graphics_line_draw_1px_non_aa(GContext* ctx, GPoint p0, GPoint p1);

//! @internal
//! Draws antialiased 1px width line between given points, will adjust to drawing box
MOCKABLE void graphics_line_draw_1px_aa(GContext* ctx, GPoint p0, GPoint p1);

//! @internal
//! Draws antialiased stroked line between given points, will adjust for drawing_box
//! @note This only supports odd numbers for stroke_width - even numbers will be rounded up.
//! Minimal supported stroke_width is 3
MOCKABLE void graphics_line_draw_stroked_aa(GContext* ctx, GPoint p0, GPoint p1,
                                            uint8_t stroke_width);

//! @internal
//! Draws non-antialiased stroked line between given precise points, will adjust for drawing_box
//! Minimal supported stroke_width is 2
MOCKABLE void graphics_line_draw_precise_stroked_non_aa(GContext* ctx, GPointPrecise p0,
                                                        GPointPrecise p1, uint8_t stroke_width);

//! @internal
//! Draws antialiased stroked line between given precise points, will adjust for drawing_box
//! Minimal supported stroke_width is 2
MOCKABLE void graphics_line_draw_precise_stroked_aa(GContext* ctx, GPointPrecise p0,
                                                    GPointPrecise p1, uint8_t stroke_width);

//! @internal
//! Draws non-antialiased stroked line between given point, will adjust for drawing_box
//! @note This only supports odd numbers for stroke_width - even numbers will be rounded up.
//! Minimal supported stroke_width is 3
MOCKABLE void graphics_line_draw_stroked_non_aa(GContext* ctx, GPoint p0, GPoint p1,
                                                uint8_t stroke_width);

//! @internal
//! Draws stroked line between given precise points, will adjust for drawing_box,
//! current stroke color, current stroke width and AA flag
//! Minimal supported stroke_width is 2
void graphics_line_draw_precise_stroked(GContext* ctx, GPointPrecise p0, GPointPrecise p1);

//! @internal
//! Draws a 1 pixel wide non-antialiased vertical dotted line of length pixels starting at p0.
//! Will draw the line in the positive y direction. Will adjust for drawing_box.
void graphics_draw_vertical_line_dotted(GContext* ctx, GPoint p0, uint16_t length);

//! @internal
//! Draws a 1 pixel high non-antialiased horizontal dotted line of length pixels starting at p0.
//! Will draw the line in the positive x direction. Will adjust for drawing_box.
void graphics_draw_horizontal_line_dotted(GContext* ctx, GPoint p0, uint16_t length);
