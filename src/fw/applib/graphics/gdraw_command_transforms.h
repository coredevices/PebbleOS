/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gdraw_command_image.h"
#include "applib/graphics/gdraw_command_sequence.h"
#include "applib/ui/animation_timing.h"
#include "util/math_fixed.h"

// GDraw Command Transforms is a collection of draw command transforms.
//
// Some transforms apply effects immediately and others are to be used in an animation.
// Transforms that are for animation and take a normalized position use the infinitive "to" as
// opposed to "animation" for brevity.
//
// Among the animation transforms, there is class that delays the animation for each of its
// participants with different delay times. These transforms are suffixed with "segmented" and
// generally time the points by using a combination of GPointIndexLookup and
// animation_timing_segmented.

//! GStrokeWidthOp specifies the different types of operations to perform during a stroke
//! width transform. Stroke width transformation takes a start and an end, so combining two
//! operators can result in your desired animation. Each operation is paired with a value to
//! operate along with the native stroke width. For example, if you want to start from a circle
//! of diameter 10px and transform to 2x the native stroke width, start with GStrokeWidthOpSet of 10
//! and end with GStrokeWidthOpMultiply of 2.
typedef enum {
  //! Sets the stroke width to the paired operation value, overriding the native stroke width
  GStrokeWidthOpSet,
  //! Multiplies the native stroke width with the paired operation value, scaling the stroke width
  GStrokeWidthOpMultiply,
  //! Adds the paired operation value to the native stroke width
  GStrokeWidthOpAdd,
} GStrokeWidthOp;

//! A GPointIndexLookup is used for segmented animations.
//! Segmented animations are where participating elements have a delayed start compared to other
//! elements in the same animation. Each element has the same animation time, so earlier elements
//! complete their animation sooner than others.
//! GPointIndexLookup is a lookup array with the mapping (GPoint index => animation index).
//! The animation index is used as the delay multiple in segmented animations.
//! The delay multiple is how many delay segments the particular GPoint must wait before it is
//! transformed.
//! @see animation_timing_segmented
typedef struct {
  uint16_t max_index;
  uint16_t num_points;
  uint16_t index_lookup[];
} GPointIndexLookup;

//! Scales a list from one size to another
void gdraw_command_list_scale(GDrawCommandList *list, GSize from, GSize to);

//! Scales an image to a given size
void gdraw_command_image_scale(GDrawCommandImage *image, GSize to);

//! Attracts points of a list to a square
void gdraw_command_list_attract_to_square(GDrawCommandList *list, GSize size, int32_t normalized);

//! Attracts points of an image to a square
void gdraw_command_image_attract_to_square(GDrawCommandImage *image, int32_t normalized);
GPoint gpoint_attract_to_square(GPoint point, GSize size, int32_t normalized);

//! Creates a GPointIndexLookup based on the angle to the center of an image
//! Points in the image whose ray with the image's center has a smaller angle are animated first.
//! @param angle Angle at which to consider zero. Points at this angle animate first.
//! @see GPointIndexLookup
GPointIndexLookup *gdraw_command_list_create_index_lookup_by_angle(GDrawCommandList *list,
                                                                   GPoint origin, int32_t angle);

//! Creates a GPointIndexLookup based on distance to a target GPoint.
//! Points in the image that are closer to the target are given the lowest animation index and
//! are therefore animated first.
//! To obtain a stretching animation, select a target among the points in a image's perimeter that
//! is most closest to its destination animation point.
//! Choosing a target in the image's perimeter opposite of the destination animation point results
//! in a paper flipping effect.
//! @param target Point to compare against in image coordinates. (0, 0) is top left.
//! @see GPointIndexLookup
GPointIndexLookup *gdraw_command_list_create_index_lookup_by_distance(GDrawCommandList *list,
                                                                      GPoint target);

//! Shifts the delay index of all points at or above a given delay index.
//! \note This shifts the delay index up, so be sure to insert the last most delays first.
//! @param lookup GPointIndexLookup to add delay to
//! @param index Delay index to add delay to
//! @param amount Amount of delay to add in index units
//! @see GPointIndexLookup
void gpoint_index_lookup_add_at(GPointIndexLookup *lookup, int delay_index, int delay_amount);

//! Adds delay between the groups that the lookup is desired to be partitioned into. The groups
//! are partitioned evenly by number of points.
//! @param lookup GPointIndexLookup to add delay to
//! @param num_groups Number of groups to partition by
//! @param group_delay Amount of additional delay to add between each group proportional to the
//! animation duration of one group
//! @see GPointIndexLookup
void gpoint_index_lookup_set_groups(GPointIndexLookup *lookup, int num_groups,
                                    Fixed_S32_16 group_delay);

//! Performs a scaling and translation transform on a list with each point being delayed by delay
//! segments assigned based on a GPointIndexLookup.
//! @param size Native size of the points within the command list. This is used for scaling.
//! @param from Position and size to start from in local drawing coordinates.
//! @param to Position and size to end at in local drawing coordinates.
//! @param normalized Normalized animation position to transform to.
//! @param interpolate InterpolateInt64Function to apply to each point individually
//! @param lookup \ref GPointIndexLookup delay index that each point's delay is derived from.
//! @param duration_fraction \ref animation_timing_segmented animation duration that each
//! point would animate in within the animation's duration.
//! @param is_offset true if the command list has already been offset another transform. When true,
//! this prevents the transform from scaling the translation already present in the command list
//! equivalent to `from.origin`.
//! @see GPointIndexLookup
void gdraw_command_list_scale_segmented_to(
    GDrawCommandList *list, GSize size, GRect from, GRect to, AnimationProgress normalized,
    InterpolateInt64Function interpolate, GPointIndexLookup *lookup, Fixed_S32_16 duration_fraction,
    bool is_offset);

//! Performs a scaling and translation transform on an image with each point being delayed by delay
//! segments assigned based on a GPointIndexLookup.
//! @param from Position and size to start from in local drawing coordinates.
//! @param to Position and size to end at from in local drawing coordinates.
//! @param normalized Normalized animation position to transform to.
//! @param interpolate InterpolateInt64Function to apply to each point individually
//! @param lookup \ref GPointIndexLookup delay index that each point's delay is derived from.
//! @param duration_fraction \ref animation_timing_segmented animation duration that each
//! point would animate in within the animation's duration.
//! @param is_offset true if the command list has already been offset another transform. When true,
//! this prevents the transform from scaling the translation already present in the command list
//! equivalent to `from.origin`.
//! @see GPointIndexLookup
void gdraw_command_image_scale_segmented_to(
    GDrawCommandImage *image, GRect from, GRect to, AnimationProgress normalized,
    InterpolateInt64Function interpolate, GPointIndexLookup *lookup, Fixed_S32_16 duration_fraction,
    bool is_offset);

//! Scales and translates a GPoint.
//! @param point Point to transform.
//! @param size Dimensions of the canvas or image the point belongs to.
//! @param from Position and size to start from in local drawing coordinates.
//! @param to Position and size to end at from in local drawing coordinates.
//! @param normalized Normalized animation position to transform to.
//! @param interpolate InterpolateInt64Function to use for interpolation.
GPoint gpoint_scale_to(GPoint point, GSize size, GRect from, GRect to, int32_t normalized,
                       InterpolateInt64Function interpolate);

//! Transforms the stroke width of a list as defined by a pair of GStrokeWidthOp.
//! @param list GDrawCommandList to scale stroke width of
//! @param from Fixed_S16_3 From stroke width operator value
//! @param to Fixed_S16_3 To stroke width operator value
//! @param from_op GStrokeWidthOp operation to start with
//! @param to_op GStrokeWidthOp operation to end with
//! @param progress AnimationProgress position of the transform
//! @see GStrokeWidthOp
void gdraw_command_list_scale_stroke_width(GDrawCommandList *list, Fixed_S16_3 from, Fixed_S16_3 to,
                                           GStrokeWidthOp from_op, GStrokeWidthOp to_op,
                                           AnimationProgress progress);

//! Transforms the stroke width of an image as defined by a pair of GStrokeWidthOp.
//! @param image GDrawCommandImage to scale stroke width of
//! @param from Fixed_S16_3 From stroke width operator value
//! @param to Fixed_S16_3 To stroke width operator value
//! @param from_op GStrokeWidthOp operation to start with
//! @param to_op GStrokeWidthOp operation to end with
//! @param progress AnimationProgress position of the transform
//! @see GStrokeWidthOp
void gdraw_command_image_scale_stroke_width(GDrawCommandImage *image, Fixed_S16_3 from,
                                            Fixed_S16_3 to, GStrokeWidthOp from_op,
                                            GStrokeWidthOp to_op, AnimationProgress progress);

void gdraw_command_frame_replace_color(GDrawCommandFrame *frame, GColor from, GColor to);
void gdraw_command_replace_color(GDrawCommand *command, GColor from, GColor to);
