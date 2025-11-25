/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "applib/graphics/graphics.h"

#include <stdint.h>
#include <stdbool.h>

//! @file graphics/gdraw_command.h
//! Defines the basic functions available to manipulate Pebble Draw Commands
//! @addtogroup Graphics
//! @{
//!   @addtogroup DrawCommand Draw Commands
//! \brief Pebble Draw Commands are a way to encode arbitrary path draw and fill calls in binary
//! format, so that vector-like graphics can be represented on the watch.
//!
//! These draw commands can
//! be loaded from resources, manipulated in place and drawn to the current graphics context. Each
//! \ref GDrawCommand can be an arbitrary path or a circle with optional fill or stroke. The stroke
//! width and color of the stroke and fill are also encoded within the \ref GDrawCommand. Paths can
//! can be drawn open or closed.
//!
//! All aspects of a draw command can be modified, except for the number of points in a path (a
//! circle only has one point, the center).
//!
//! Draw commands are grouped into a \ref GDrawCommandList, which can be drawn all at once.
//! Each individual \ref GDrawCommand can be accessed from a \ref GDrawCommandList for modification.
//!
//! A \ref GDrawCommandList forms the basis for \ref GDrawCommandImage and \ref GDrawCommandFrame
//! objects. A \ref GDrawCommandImage represents a static image and can be represented by the PDC
//! file format and can be loaded as a resource.
//!
//! Once you have a \ref GDrawCommandImage loaded in memory you can draw it on the screen in a
//! \ref LayerUpdateProc with the \ref gdraw_command_image_draw().
//!
//! A \ref GDrawCommandFrame represents a single frame of an animated sequence, with multiple frames
//! making up a single \ref GDrawCommandSequence, which can also be stored as a PDC and loaded as a
//! resource.
//!
//! To draw a \ref GDrawCommandSequence, use the \ref gdraw_command_sequence_get_frame_by_elapsed()
//! to obtain the current \ref GDrawCommandFrame and \ref gdraw_command_frame_draw() to draw it.
//!
//! Draw commands also allow access to drawing with sub-pixel precision. The points are treated as
//! Fixed point types in the format 13.3, so that 1/8th of a pixel precision is possible. Only the
//! points in draw commands of the type GDrawCommandTypePrecisePath will be treated as higher
//! precision.
//!
//! @{

typedef enum {
  GDrawCommandTypeInvalid = 0,  //!< Invalid draw command type
  GDrawCommandTypePath,         //!< Arbitrary path draw command type
  GDrawCommandTypeCircle,       //!< Circle draw command type
  GDrawCommandTypePrecisePath,  //!< Arbitrary path drawn with sub-pixel precision (1/8th precision)
} GDrawCommandType;

struct GDrawCommand;

//! Draw commands are the basic building block of the draw command system, encoding the type of
//! command to draw, the stroke width and color, fill color, and points that define the path (or
//! center of a circle
typedef struct GDrawCommand GDrawCommand;

//! @internal
//! Use to check the file signature on a PDC resource
bool gdraw_command_resource_is_valid(ResAppNum res_app, uint32_t resource_id,
                                     uint32_t expected_signature, uint32_t *data_size);

//! @internal
//! Use to validate data stored as a draw command
bool gdraw_command_validate(GDrawCommand *command, size_t size);

//! Draw a command
//! @param ctx The destination graphics context in which to draw
//! @param command \ref GDrawCommand to draw
void gdraw_command_draw(GContext *ctx, GDrawCommand *command);

//! @internal
//! Get the size of a command in memory
size_t gdraw_command_get_data_size(GDrawCommand *command);

//! Get the command type
//! @param command \ref GDrawCommand from which to get the type
//! @return The type of the given \ref GDrawCommand
GDrawCommandType gdraw_command_get_type(GDrawCommand *command);

//! Set the fill color of a command
//! @param command ref DrawCommand for which to set the fill color
//! @param fill_color \ref GColor to set for the fill
void gdraw_command_set_fill_color(GDrawCommand *command, GColor fill_color);

//! Get the fill color of a command
//! @param command \ref GDrawCommand from which to get the fill color
//! @return fill color of the given \ref GDrawCommand
GColor gdraw_command_get_fill_color(GDrawCommand *command);

//! Set the stroke color of a command
//! @param command \ref GDrawCommand for which to set the stroke color
//! @param stroke_color \ref GColor to set for the stroke
void gdraw_command_set_stroke_color(GDrawCommand *command, GColor stroke_color);

//! Get the stroke color of a command
//! @param command \ref GDrawCommand from which to get the stroke color
//! @return The stroke color of the given \ref GDrawCommand
GColor gdraw_command_get_stroke_color(GDrawCommand *command);

//! Set the stroke width of a command
//! @param command \ref GDrawCommand for which to set the stroke width
//! @param stroke_width stroke width to set for the command
void gdraw_command_set_stroke_width(GDrawCommand *command, uint8_t stroke_width);

//! Get the stroke width of a command
//! @param command \ref GDrawCommand from which to get the stroke width
//! @return The stroke width of the given \ref GDrawCommand
uint8_t gdraw_command_get_stroke_width(GDrawCommand *command);

//! Get the number of points in a command
uint16_t gdraw_command_get_num_points(GDrawCommand *command);

//! Set the value of the point in a command at the specified index
//! @param command \ref GDrawCommand for which to set the value of a point
//! @param point_idx Index of the point to set the value for
//! @param point new point value to set
void gdraw_command_set_point(GDrawCommand *command, uint16_t point_idx, GPoint point);

//! Get the value of a point in a command from the specified index
//! @param command \ref GDrawCommand from which to get a point
//! @param point_idx The index to get the point for
//! @return The point in the \ref GDrawCommand specified by point_idx
//! @note The index \b must be less than the number of points
GPoint gdraw_command_get_point(GDrawCommand *command, uint16_t point_idx);

//! Set the radius of a circle command
//! @note This only works for commands of type \ref GDrawCommandCircle
//! @param command \ref GDrawCommand from which to set the circle radius
//! @param radius The radius to set for the circle.
void gdraw_command_set_radius(GDrawCommand *command, uint16_t radius);

//! Get the radius of a circle command.
//! @note this only works for commands of type\ref GDrawCommandCircle.
//! @param command \ref GDrawCommand from which to get the circle radius
//! @return The radius in pixels if command is of type \ref GDrawCommandCircle
uint16_t gdraw_command_get_radius(GDrawCommand *command);

//! Set the path of a stroke command to be open
//! @note This only works for commands of type \ref GDrawCommandPath and
//! \ref GDrawCommandPrecisePath
//! @param command \ref GDrawCommand for which to set the path open status
//! @param path_open true if path should be hidden
void gdraw_command_set_path_open(GDrawCommand *command, bool path_open);

//! Return whether a stroke command path is open
//! @note This only works for commands of type \ref GDrawCommandPath and
//! \ref GDrawCommandPrecisePath
//! @param command \ref GDrawCommand from which to get the path open status
//! @return true if the path is open
bool gdraw_command_get_path_open(GDrawCommand *command);

//! Set a command as hidden. This command will not be drawn when \ref gdraw_command_draw is called
//! with this command
//! @param command \ref GDrawCommand for which to set the hidden status
//! @param hidden true if command should be hidden
void gdraw_command_set_hidden(GDrawCommand *command, bool hidden);

//! Return whether a command is hidden
//! @param command \ref GDrawCommand from which to get the hidden status
//! @return true if command is hidden
bool gdraw_command_get_hidden(GDrawCommand *command);

//! @internal
//! Copy the points from command to a given buffer
//! The buffer should be at least the number points * sizeof(GPoint)
//! @param points the points buffer GPoints will be copied into
//! @param max_bytes the points buffer size
//! Use gdraw_command_get_num_points to correctly size the buffer
//! @return the amount of bytes that were copied
size_t gdraw_command_copy_points(GDrawCommand *command, GPoint *points, const size_t max_bytes);

//!   @} // end addtogroup DrawCommand
//! @} // end addtogroup Graphics
