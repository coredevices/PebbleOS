/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "gtypes.h"

//! Draws a bitmap into the graphics context, inside the specified rectangle, using the specified
//! processor.
//! @param ctx The destination graphics context in which to draw the bitmap
//! @param bitmap The bitmap to draw
//! @param rect The rectangle in which to draw the bitmap
//! @param processor Optional processor to use in drawing the bitmap
//! @note If the size of `rect` is smaller than the size of the bitmap,
//! the bitmap will be clipped on right and bottom edges.
//! If the size of `rect` is larger than the size of the bitmap,
//! the bitmap will be tiled automatically in both horizontal and vertical
//! directions, effectively drawing a repeating pattern.
//! @see GBitmap
//! @see GContext
//! @internal
//! @see app_get_current_graphics_context
void graphics_draw_bitmap_in_rect_processed(GContext *ctx, const GBitmap *bitmap,
                                            const GRect *rect, GBitmapProcessor *processor);

//! Draws a bitmap into the graphics context, inside the specified rectangle
//! @param ctx The destination graphics context in which to draw the bitmap
//! @param bitmap The bitmap to draw
//! @param rect The rectangle in which to draw the bitmap
//! @note If the size of `rect` is smaller than the size of the bitmap,
//! the bitmap will be clipped on right and bottom edges.
//! If the size of `rect` is larger than the size of the bitmap,
//! the bitmap will be tiled automatically in both horizontal and vertical
//! directions, effectively drawing a repeating pattern.
//! @see GBitmap
//! @see GContext
//! @internal
//! @see app_get_current_graphics_context
void graphics_draw_bitmap_in_rect_by_value(GContext *ctx, const GBitmap *bitmap, GRect rect);
void graphics_draw_bitmap_in_rect(GContext *ctx, const GBitmap *bitmap, const GRect *rect);

//! Draws a rotated bitmap with a memory-sensitive 2x anti-aliasing technique
//! (using ray-finding instead of super-sampling), which is thresholded into a b/w bitmap for 1-bit
//! and color blended for 8-bit.
//! @note This API has performance limitations that can degrade user experience. Use sparingly.
//! @param ctx The destination graphics context in which to draw
//! @param src The source bitmap to draw
//! @param src_ic Instance center (single point unaffected by rotation) relative to source bitmap
//! @param rotation Angle of rotation. Rotation is an integer between 0 (no rotation)
//! and TRIG_MAX_ANGLE (360 degree rotation). Use \ref DEG_TO_TRIGANGLE to easily convert degrees
//! to the appropriate value.
//! @param dest_ic Where to draw the instance center of the rotated bitmap in the context.
void graphics_draw_rotated_bitmap(GContext* ctx, GBitmap *src, GPoint src_ic, int rotation,
                                  GPoint dest_ic);
