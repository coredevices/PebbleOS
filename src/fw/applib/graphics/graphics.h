/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "gcontext.h"
#include "gtypes.h"
#include "graphics_bitmap.h"
#include "graphics_circle.h"
#include "graphics_line.h"

//! @file graphics/graphics.h
//! Defines the base graphics subsystem including the screen buffer. Users of these
//! functions should call graphics_set_pixel to draw to the memory-backed buffer, and
//! then graphics_flush to actually apply these changes to the display.

//! @addtogroup Graphics
//! @{

typedef struct FrameBuffer FrameBuffer;

//!   @addtogroup Drawing Drawing Primitives
//! \brief Functions to draw into a graphics context
//!
//! Use these drawing functions inside a Layer's `.update_proc` drawing
//! callback. A `GContext` is passed into this callback as an argument.
//! This `GContext` can then be used with all of the drawing functions which
//! are documented below.
//! See \ref GraphicsContext for more information about the graphics context.
//!
//! Refer to \htmlinclude UiFramework.html (chapter "Layers" and "Graphics") for a
//! conceptual overview of the drawing system, Layers and relevant code examples.
//!
//! Other drawing functions and related documentation:
//! * \ref TextDrawing
//! * \ref PathDrawing
//! * \ref GraphicsTypes
//!   @{

//! Draws a pixel at given point in the current stroke color
//! @param ctx The destination graphics context in which to draw
//! @param point The point at which to draw the pixel
void graphics_draw_pixel(GContext* ctx, GPoint point);

//! Fills a rectangle with the current fill color
//! @param ctx The destination graphics context in which to draw
//! @param rect The rectangle to fill
//! @see graphics_fill_round_rect
void graphics_fill_rect(GContext *ctx, const GRect *rect);

//! Draws a 1-pixel wide rectangle outline in the current stroke color
//! @param ctx The destination graphics context in which to draw
//! @param rect The rectangle for which to draw the outline
void graphics_draw_rect_by_value(GContext *ctx, GRect rect);
void graphics_draw_rect(GContext *ctx, const GRect *rect);
void graphics_draw_rect_precise(GContext* ctx, const GRectPrecise *rect);

//! Fills a rectangle with the current fill color, optionally rounding all or a
//! selection of its corners.
//! @param ctx The destination graphics context in which to draw
//! @param rect The rectangle to fill
//! @param corner_radius The rounding radius of the corners in pixels (maximum is 8 pixels)
//! @param corner_mask Bitmask of the corners that need to be rounded.
//! @see \ref GCornerMask
void graphics_fill_round_rect_by_value(GContext *ctx, GRect rect, uint16_t corner_radius,
                                       GCornerMask corner_mask);
void graphics_fill_round_rect(GContext *ctx, const GRect *rect, uint16_t corner_radius,
                              GCornerMask corner_mask);


//! Draws the outline of a rounded rectangle in the current stroke color
//! @param ctx The destination graphics context in which to draw
//! @param rect The rectangle defining the dimensions of the rounded rectangle to draw
//! @param radius The corner radius in pixels
void graphics_draw_round_rect_by_value(GContext *ctx, GRect rect, uint16_t radius);
void graphics_draw_round_rect(GContext *ctx, const GRect *rect, uint16_t radius);

//! Whether or not the frame buffer has been captured by {@link graphics_capture_frame_buffer}.
//! Graphics functions will not affect the frame buffer until it has been released by
//! {@link graphics_release_frame_buffer}.
//! @param ctx The graphics context providing the frame buffer
//! @return True if the frame buffer has been captured
bool graphics_frame_buffer_is_captured(GContext* ctx);

//! Captures the frame buffer for direct access, using the given format.
//! Graphics functions will not affect the frame buffer while it is captured.
//! The frame buffer is released when {@link graphics_release_frame_buffer} is called.
//! The frame buffer must be released before the end of a layer's `.update_proc`
//! for the layer to be drawn properly.
//!
//! While the frame buffer is captured calling {@link graphics_capture_frame_buffer}
//! will fail and return `NULL`.
//! @note When writing to the frame buffer, you should respect the visible boundaries of a
//! window on the screen. Use layer_get_frame(window_get_root_layer(window)).origin to obtain its
//! position relative to the frame buffer. For example, drawing to (5, 5) in the frame buffer
//! while the window is transitioning to the left with its origin at (-20, 0) would
//! effectively draw that point at (25, 5) relative to the window. For this reason you should
//! consider the window's root layer frame when calculating drawing coordinates.
//! @see GBitmap
//! @see GBitmapFormat
//! @see layer_get_frame
//! @see window_get_root_layer
//! @param ctx The graphics context providing the frame buffer
//! @param format The format in which the framebuffer should be captured. Supported formats
//! are \ref GBitmapFormat1Bit and \ref GBitmapFormat8Bit.
//! @return A pointer to the frame buffer. `NULL` if failed.
GBitmap *graphics_capture_frame_buffer_format(GContext *ctx, GBitmapFormat format);

//! A shortcut to capture the framebuffer in the native format of the watch.
//! @see graphics_capture_frame_buffer_format
GBitmap* graphics_capture_frame_buffer(GContext* ctx);
GBitmap* graphics_capture_frame_buffer_2bit(GContext* ctx);

//! Releases the frame buffer.
//! Must be called before the end of a layer's `.update_proc` for the layer to be drawn properly.
//!
//! If `buffer` does not point to the address previously returned by
//! {@link graphics_capture_frame_buffer} the frame buffer will not be released.
//! @param ctx The graphics context providing the frame buffer
//! @param buffer The pointer to frame buffer
//! @return True if the frame buffer was released successfully
bool graphics_release_frame_buffer(GContext* ctx, GBitmap* buffer);

//!   @} // end addtogroup Drawing
//! @} // end addtogroup Graphics
