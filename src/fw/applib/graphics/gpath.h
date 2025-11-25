/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "gtypes.h"

//! @addtogroup Graphics
//! @{
//!   @addtogroup PathDrawing Drawing Paths
//! \brief Functions to draw polygons into a graphics context
//!
//! Code example:
//! \code{.c}
//! static GPath *s_my_path_ptr = NULL;
//!
//! static const GPathInfo BOLT_PATH_INFO = {
//!   .num_points = 6,
//!   .points = (GPoint []) {{21, 0}, {14, 26}, {28, 26}, {7, 60}, {14, 34}, {0, 34}}
//! };
//!
//! // .update_proc of my_layer:
//! void my_layer_update_proc(Layer *my_layer, GContext* ctx) {
//!   // Fill the path:
//!   graphics_context_set_fill_color(ctx, GColorWhite);
//!   gpath_draw_filled(ctx, s_my_path_ptr);
//!   // Stroke the path:
//!   graphics_context_set_stroke_color(ctx, GColorBlack);
//!   gpath_draw_outline(ctx, s_my_path_ptr);
//! }
//!
//! void setup_my_path(void) {
//!   s_my_path_ptr = gpath_create(&BOLT_PATH_INFO);
//!   // Rotate 15 degrees:
//!   gpath_rotate_to(s_my_path_ptr, TRIG_MAX_ANGLE / 360 * 15);
//!   // Translate by (5, 5):
//!   gpath_move_to(s_my_path_ptr, GPoint(5, 5));
//! }
//!
//! // For brevity, the setup of my_layer is not written out...
//! \endcode
//!   @{

//! Data structure describing a naked path
//! @note Note that this data structure only refers to an array of points;
//! the points are not stored inside this data structure itself.
//! In most cases, one cannot use a stack-allocated array of GPoints. Instead
//! one often needs to provide longer-lived (static or "global") storage for the points.
typedef struct GPathInfo {
  //! The number of points in the `points` array
  uint32_t num_points;
  //! Pointer to an array of points.
  GPoint *points;
} GPathInfo;

//! Data structure describing a path, plus its rotation and translation.
//! @note See the remark with \ref GPathInfo
typedef struct GPath {
  //! The number of points in the `points` array
  uint32_t num_points;
  //! Pointer to an array of points.
  GPoint *points;
  //! The rotation that will be used when drawing the path with
  //! \ref gpath_draw_filled() or \ref gpath_draw_outline()
  int32_t rotation;
  //! The translation that will to be used when drawing the path with
  //! \ref gpath_draw_filled() or \ref gpath_draw_outline()
  GPoint offset;
} GPath;

//! @internal
//! Initializes a GPath based on a series of points described by a GPathInfo.
void gpath_init(GPath *path, const GPathInfo *init);

//! Creates a new GPath on the heap based on a series of points described by a GPathInfo.
//!
//! Values after initialization:
//! * `num_points` and `points` pointer: copied from the GPathInfo.
//! * `rotation`: 0
//! * `offset`: (0, 0)
//! @return A pointer to the GPath. `NULL` if the GPath could not
//! be created
GPath* gpath_create(const GPathInfo *init);

//! Free a dynamically allocated gpath created with \ref gpath_create()
void gpath_destroy(GPath* gpath);

//! Draws the fill of a path into a graphics context, using the current fill color,
//! relative to the drawing area as set up by the layering system.
//! @param ctx The graphics context to draw into
//! @param path The path to fill
//! @see \ref graphics_context_set_fill_color()
void gpath_draw_filled(GContext* ctx, GPath *path);

//! Draws the outline of a path into a graphics context, using the current stroke color and
//! width, relative to the drawing area as set up by the layering system. The first and last points
//! in the path do have a line between them.
//! @param ctx The graphics context to draw into
//! @param path The path to draw
//! @see \ref graphics_context_set_stroke_color()
//! @see \ref gpath_draw_outline_open()
void gpath_draw_outline(GContext* ctx, GPath *path);

//! Draws an open outline of a path into a graphics context, using the current stroke color and
//! width, relative to the drawing area as set up by the layering system. The first and last points
//! in the path do not have a line between them.
//! @param ctx The graphics context to draw into
//! @param path The path to draw
//! @see \ref graphics_context_set_stroke_color()
//! @see \ref gpath_draw_outline()
void gpath_draw_outline_open(GContext* ctx, GPath* path);

//! @internal
//! Draws a stroke following a path into a graphics context, using the current stroke color and
//! width, relative to the drawing area as set up by the layering system.
//! @param ctx The graphics context to draw into
//! @param path The path to draw
//! @param open true if path must be left open (not closed between first and last points)
//! @see \ref graphics_context_set_stroke_color()
void gpath_draw_stroke(GContext* ctx, GPath *path, bool open);

//! Sets the absolute rotation of the path.
//! The current rotation will be replaced by the specified angle.
//! @param path The path onto which to set the rotation
//! @param angle The absolute angle of the rotation. The angle is represented in the same way
//! that is used with \ref sin_lookup(). See \ref TRIG_MAX_ANGLE for more information.
//! @note Setting a rotation does not affect the points in the path directly.
//! The rotation is applied on-the-fly during drawing, either using \ref gpath_draw_filled() or
//! \ref gpath_draw_outline().
void gpath_rotate_to(GPath *path, int32_t angle);

//! Applies a relative rotation to the path.
//! The angle will be added to the current rotation of the path.
//! @param path The path onto which to apply the rotation
//! @param delta_angle The relative angle of the rotation. The angle is represented in the same way
//! that is used with \ref sin_lookup(). See \ref TRIG_MAX_ANGLE for more information.
//! @note Applying a rotation does not affect the points in the path directly.
//! The rotation is applied on-the-fly during drawing, either using \ref gpath_draw_filled() or
//! \ref gpath_draw_outline().
void gpath_rotate(GPath *path, int32_t delta_angle);

//! Sets the absolute offset of the path.
//! The current translation will be replaced by the specified offset.
//! @param path The path onto which to set the translation
//! @param point The point which is used as the vector for the translation.
//! @note Setting a translation does not affect the points in the path directly.
//! The translation is applied on-the-fly during drawing, either using \ref gpath_draw_filled() or
//! \ref gpath_draw_outline().
void gpath_move_to(GPath *path, GPoint point);

//! Applies a relative offset to the path.
//! The offset will be added to the current translation of the path.
//! @param path The path onto which to apply the translation
//! @param delta The point which is used as the vector for the translation.
//! @note Applying a translation does not affect the points in the path directly.
//! The translation is applied on-the-fly during drawing, either using \ref gpath_draw_filled() or
//! \ref gpath_draw_outline().
void gpath_move(GPath *path, GPoint delta);

//! Calculates the outer rectangle of the path's points,
//! ignoring the offset and rotation that might be set.
GRect gpath_outer_rect(GPath *path);

//! @internal
//! Drawing function callback
//! @param ctx GContext of drawing
//! @param y integral Y coordinate of drawn line
//! @param x_range_begin precise X coordinate of beginning of the line
//! @param x_range_end precise X coordinate of ending of the line
//! @param delta_begin Delta of the line crossing x_range_begin - negative if no AA
//! @param delta_end Delta of the line crossing x_range_end - negative if no AA
//! @param user_data User data for extra data the callback may require
typedef void (*GPathDrawFilledCallback)(
    GContext *ctx, int16_t y, Fixed_S16_3 x_range_begin, Fixed_S16_3 x_range_end,
    Fixed_S16_3 delta_begin, Fixed_S16_3 delta_end, void *user_data);

//! @internal
//! Allows for customized drawing of a GContext's drawing_box with a GPath defining "inside" and
//! "outside" regions. Nothing is drawn by this method, all drawing should be done by the supplied
//! callback.
void gpath_draw_filled_with_cb(GContext *ctx, GPath *path, GPathDrawFilledCallback cb,
                               void *user_data);

//! @internal
void gpath_fill_precise_internal(GContext *ctx, GPointPrecise *points, size_t num_points);

//! @internal
void gpath_draw_outline_precise_internal(GContext *ctx, GPointPrecise *points, size_t num_points,
                                         bool open);

//!   @} // end addtogroup PathDrawing
//! @} // end addtogroup Graphics
