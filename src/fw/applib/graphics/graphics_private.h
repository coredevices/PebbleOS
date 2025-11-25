/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "gtypes.h"

#define MAX_PLOT_BRIGHTNESS 3
#define MAX_PLOT_OPACITY 0
#define MAX_RADIUS_LOOKUP 13

//! Plots pixel at given coordinates
//! Note this does not adjust to drawing_box!
//! @internal
//! @param ctx Graphics context for drawing
//! @param point Point to set pixel at using draw state's stroke color
void graphics_private_set_pixel(GContext* ctx, GPoint point);

//! Draws horizontal line with antialiased starting and ending pixel
//! Will adjust to the drawing_box and clip_box
//! Note: this only works for lines where x1 < x2
//! @param ctx Graphics context for drawing
//! @param y Integral Y coordinate for line
//! @param x1 Fixedpoint X coordinate for starting point
//! @param x2 Fixedpoint X coordinate for ending point
//! @internal
void graphics_private_draw_horizontal_line(GContext *ctx, int16_t y, Fixed_S16_3 x1,
                                            Fixed_S16_3 x2);

//! Draws horizontal line into framebuffer, requires adjustment for drawing_box and clip_box
//! @param ctx Graphics context for drawing
//! @param y Integral Y coordinate for line
//! @param x1 Integral X coordinate for starting point
//! @param x2 Integral X coordinate for ending point
//! @param color Color to be used
//! @internal
void graphics_private_draw_horizontal_line_integral(GContext *ctx, GBitmap *framebuffer, int16_t y,
                                                    int16_t x1, int16_t x2, GColor color);

//! Draws vertical line with antialiased starting and ending pixel
//! Will adjust to the drawing_box and clip_box
//! Note: this only works for lines where y1 < y2
//! @param ctx Graphics context for drawing
//! @param x Integral X coordinate for line
//! @param y1 Fixedpoint Y coordinate for starting point
//! @param y2 Fixedpoint Y coordinate for ending point
//! @internal
void graphics_private_draw_vertical_line(GContext *ctx, int16_t x, Fixed_S16_3 y1, Fixed_S16_3 y2);

//! Draws horizontal line with antialiased starting and ending pixel
//! Will use clip_box for clipping
//! Note: this does not adjust for drawing_box
//! Note: this only works for lines where x1 < x2
//! @param ctx Graphics context for drawing
//! @param y Integral Y coordinate for line
//! @param x1 Fixedpoint X coordinate for starting point
//! @param x2 Fixedpoint X coordinate for ending point
//! @internal
void graphics_private_draw_horizontal_line_prepared(GContext *ctx, GBitmap *framebuffer,
                                                    GRect *clip_box, int16_t y, Fixed_S16_3 x1,
                                                    Fixed_S16_3 x2, GColor color);

//! Draws vertical line with antialiased starting and ending pixel
//! Will use clip_box for clipping
//! Note: this does not adjust for drawing_box
//! Note: this only works for lines where y1 < y2
//! @param ctx Graphics context for drawing
//! @param x Integral X coordinate for line
//! @param y1 Fixedpoint Y coordinate for starting point
//! @param y2 Fixedpoint Y coordinate for ending point
//! @internal
void graphics_private_draw_vertical_line_prepared(GContext *ctx, GBitmap *framebuffer,
                                                  GRect *clip_box, int16_t x, Fixed_S16_3 y1,
                                                  Fixed_S16_3 y2, GColor color);

//! Blends pixel at given coordinates into given bitmap (framebuffer)
//! Will use given clip_box for clipping
//! Note: this will not adjust for drawing_box
//! @param ctx Graphics context for plotting
//! @param framebuffer Address of framebuffer to plot pixel into
//! @param clip_box Address of clipping rectangle to perform clipping check
//! @param x Integral X coordinate of the point
//! @param y Integral Y coordinate of the point
//! @param opacity Value that will be reverted and applied to alpha channel
//! @param color Color of the pixel to blend
//! @internal
void graphics_private_plot_pixel(GBitmap *framebuffer, GRect *clip_box, int x, int y,
                                 uint16_t opacity, GColor color);

//! Blends horizontal line between given points using current stroke color
//! Will adjust to drawing_box and clip_box
//! @param ctx Graphics context for plotting
//! @param y Y coordinate of line
//! @param x1 Starting point for the line
//! @param x2 Ending point for the line
//! @param opacity Value that will be reverted and applied to alpha channel
//!        if off this will just revert to regular line with full opacity
void graphics_private_plot_horizontal_line(GContext *ctx, int16_t y, Fixed_S16_3 x1, Fixed_S16_3 x2,
                                           uint16_t opacity);

//! Blends vertical line between given points using current stroke color
//! Will adjust to drawing_box and clip_box
//! @param ctx Graphics context for plotting
//! @param x X coordinate of line
//! @param y1 Starting point for the line
//! @param y2 Ending point for the line
//! @param opacity Value that will be reverted and applied to alpha channel
//!        if off this will just revert to regular line with full opacity
void graphics_private_plot_vertical_line(GContext *ctx, int16_t y, Fixed_S16_3 y1, Fixed_S16_3 y2,
                                         uint16_t opacity);

//! Blending of vertical line used in gpath filling algorithm
void graphics_private_draw_horizontal_line_delta_aa(GContext *ctx, int16_t y, Fixed_S16_3 x1,
                                                    Fixed_S16_3 x2, Fixed_S16_3 delta1,
                                                    Fixed_S16_3 delta2);
//! duplicates the outer-most pixel from a current rectangle to fill a GContext as if that
//! rectangle moved from prev_x to current.origin.x
//! will update prev_x afterwards
void graphics_patch_trace_of_moving_rect(GContext *ctx, int16_t *prev_x, GRect current);

//! will move all pixels in the bitmap by delta_x.
//! @param delta_x Number of pixels to move. Positive is right, negative is left.
//! @param patch_garbage If set, will fill the undefined pixels with the edge-most color.
void graphics_private_move_pixels_horizontally(GBitmap *bitmap, int16_t delta_x,
                                               bool patch_garbage);

//! will move all pixels in the bitmap by delta_y - they will leave a trace of undefined pixels
//! @param delta_y Number of pixels to move. Positive is down, negative is up.
void graphics_private_move_pixels_vertically(GBitmap *bitmap, int16_t delta_y);

//! Returns grayscale pattern
//! @internal
//! @param color Input color
//! @param row_number Absolute number of framebuffer row
uint32_t graphics_private_get_1bit_grayscale_pattern(GColor color, uint8_t row_number);

//! Which edge of the bitmap to sample. This is identical in order to
//! CompositorTransitionDirection, and both should be wrapped into one enum as described in
//! PBL-40961
typedef enum {
  GColorSampleEdgeUp,
  GColorSampleEdgeDown,
  GColorSampleEdgeLeft,
  GColorSampleEdgeRight,
} GColorSampleEdge;

//! Samples a line of colors for a bitmap, then returns the color it found. If it found more than
//! one color or did not sample any pixels, it will return `fallback`.
//! @internal
//! @param bitmap Bitmap to sample from
//! @param edge Which edge of the bitmap to sample
//! @param fallback The color to return if no pixels were sampled or the line was not colored
//!                 homogeneously
GColor graphics_private_sample_line_color(const GBitmap *bitmap, GColorSampleEdge edge,
                                          GColor fallback);
