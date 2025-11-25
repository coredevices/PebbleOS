/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "gtypes.h"

//! @internal
//! Draws a quadrant of a circle based on what is set in the context for stroke width and
//! antialiasing.
void graphics_circle_quadrant_draw(GContext* ctx, GPoint p, uint16_t radius, GCornerMask quadrant);

//! @internal
//! Fills an antialiased circle in quadrants
MOCKABLE void graphics_internal_circle_quadrant_fill_aa(GContext* ctx, GPoint p,
                                                        uint16_t radius, GCornerMask quadrant);

//! @internal
//! Fills a non-antialiased circle in quadrants
void graphics_circle_quadrant_fill_non_aa(GContext* ctx, GPoint p,
                                          uint16_t radius, GCornerMask quadrant);

//! @internal
//! Fills a non-antialiased circle
MOCKABLE void graphics_circle_fill_non_aa(GContext* ctx, GPoint p, uint16_t radius);

//! @internal
//! Draws an arc with fixed-point precision
void graphics_draw_arc_precise_internal(GContext *ctx, GPointPrecise center, Fixed_S16_3 radius,
                                        int32_t angle_start, int32_t angle_end);

//! @internal
//! Precise version of graphics_fill_radial_internal
void graphics_fill_radial_precise_internal(GContext *ctx, GPointPrecise center,
                                           Fixed_S16_3 radius_inner, Fixed_S16_3 radius_outer,
                                           int32_t angle_start, int32_t angle_end);

//! @addtogroup Graphics
//! @{

//!   @addtogroup Drawing Drawing Primitives
//!   @{

//! Draws the outline of a circle in the current stroke color
//! @param ctx The destination graphics context in which to draw
//! @param p The center point of the circle
//! @param radius The radius in pixels
void graphics_draw_circle(GContext* ctx, GPoint p, uint16_t radius);

//! Fills a circle in the current fill color
//! @param ctx The destination graphics context in which to draw
//! @param p The center point of the circle
//! @param radius The radius in pixels
void graphics_fill_circle(GContext* ctx, GPoint p, uint16_t radius);

//! Values to specify how a given rectangle should be used to derive an oval shape.
//! @see \ref graphics_fill_radial_internal
//! @see \ref graphics_draw_arc_internal
//! @see \ref gpoint_from_polar_internal
//! @see \ref grect_centered_from_polar
typedef enum {
  //! Places a circle at the center of the rectangle, with a diameter that matches
  //! the rectangle's shortest side.
  GOvalScaleModeFitCircle,
  //! Places a circle at the center of the rectangle, with a diameter that matches
  //! the rectangle's longest side.
  //! The circle may overflow the bounds of the rectangle.
  GOvalScaleModeFillCircle,
} GOvalScaleMode;

//! Draws a line arc clockwise between `angle_start` and `angle_end`, where 0° is
//! the top of the circle. If the difference between `angle_start` and `angle_end` is greater
//! than 360°, a full circle will be drawn.
//! @param ctx The destination graphics context in which to draw using the current
//!        stroke color and antialiasing setting.
//! @param rect The reference rectangle to derive the center point and radius (see scale_mode).
//! @param scale_mode Determines how rect will be used to derive the center point and radius.
//! @param angle_start Radial starting angle. Use \ref DEG_TO_TRIGANGLE to easily convert degrees
//! to the appropriate value.
//! @param angle_end Radial finishing angle. If smaller than `angle_start`, nothing will be drawn.
void graphics_draw_arc(GContext *ctx, GRect rect, GOvalScaleMode scale_mode,
                       int32_t angle_start, int32_t angle_end);

//! @internal
void graphics_draw_arc_internal(GContext *ctx, GPoint center, uint16_t radius, int32_t angle_start,
                                int32_t angle_end);

//! Fills a circle clockwise between `angle_start` and `angle_end`, where 0° is
//! the top of the circle. If the difference between `angle_start` and `angle_end` is greater
//! than 360°, a full circle will be drawn and filled. If `angle_start` is greater than
//! `angle_end` nothing will be drawn.
//! @note A simple example is drawing a 'Pacman' shape, with a starting angle of -225°, and
//! ending angle of 45°. By setting `inset_thickness` to a non-zero value (such as 30) this
//! example will produce the letter C.
//! @param ctx The destination graphics context in which to draw using the current
//! fill color and antialiasing setting.
//! @param rect The reference rectangle to derive the center point and radius (see scale).
//! @param scale_mode Determines how rect will be used to derive the center point and radius.
//! @param inset_thickness Describes how thick in pixels the radial will be drawn towards its
//!        center measured from the outside.
//! @param angle_start Radial starting angle. Use \ref DEG_TO_TRIGANGLE to easily convert degrees
//! to the appropriate value.
//! @param angle_end Radial finishing angle. If smaller than `angle_start`, nothing will be drawn.
void graphics_fill_radial(GContext *ctx, GRect rect, GOvalScaleMode scale_mode,
                          uint16_t inset_thickness,
                          int32_t angle_start, int32_t angle_end);

//! @internal
void graphics_fill_radial_internal(GContext *ctx, GPoint center, uint16_t radius_inner,
                                   uint16_t radius_outer, int32_t angle_start, int32_t angle_end);

//! @internal
void graphics_fill_oval(GContext *ctx, GRect rect, GOvalScaleMode scale_mode);

//! Calculates a GPoint located at the angle provided on the perimeter of a circle defined by the
//! provided GRect.
//! @param rect The reference rectangle to derive the center point and radius (see scale_mode).
//! @param scale_mode Determines how rect will be used to derive the center point and radius.
//! @param angle The angle at which the point on the circle's perimeter should be calculated.
//! Use \ref DEG_TO_TRIGANGLE to easily convert degrees to the appropriate value.
//! @return The point on the circle's perimeter.
GPoint gpoint_from_polar(GRect rect, GOvalScaleMode scale_mode, int32_t angle);

//! @internal
GPoint gpoint_from_polar_internal(const GPoint *center, uint16_t radius, int32_t angle);

//! @internal
GPointPrecise gpoint_from_polar_precise(const GPointPrecise *precise_center,
                                        uint16_t precise_radius, int32_t angle);

//! Calculates a rectangle centered on the perimeter of a circle at a given angle.
//! Use this to construct rectangles that follow the perimeter of a circle as an input for
//! \ref graphics_fill_radial_internal or \ref graphics_draw_arc_internal,
//! e.g. to draw circles every 30 degrees on a watchface.
//! @param rect The reference rectangle to derive the circle's center point and radius (see
//!        scale_mode).
//! @param scale_mode Determines how rect will be used to derive the circle's center point and
//!        radius.
//! @param angle The angle at which the point on the circle's perimeter should be calculated.
//! Use \ref DEG_TO_TRIGANGLE to easily convert degrees to the appropriate value.
//! @param size Width and height of the desired rectangle.
//! @return The rectangle centered on the circle's perimeter.
GRect grect_centered_from_polar(GRect rect, GOvalScaleMode scale_mode, int32_t angle, GSize size);

//! @internal
//! Calculates a center point and radius from a given rect and scale mode
void grect_polar_calc_values(const GRect *rect, GOvalScaleMode scale_mode, GPointPrecise *center,
                             Fixed_S16_3 *radius);

//! @internal
//! Returns a GRect with a given size that's centered at center
GRect grect_centered_internal(const GPointPrecise *center, GSize size);

//!   @} // end addtogroup Drawing
//! @} // end addtogroup Graphics
