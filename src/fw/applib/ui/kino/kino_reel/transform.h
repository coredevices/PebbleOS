/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/ui/animation.h"
#include "applib/ui/kino/kino_reel.h"

// Transform Kino Reel
// This Kino Reel is meant for plugging in transform logic. Memory management and kino reel
// compatibility is automatically handled. It is a building block for exporting GDraw Command
// Transforms as Kino Reels.

//! Transform context destructor for optionally freeing or performing any cleanup.
//! @param context User supplied context for destruction.
typedef void (*TransformDestructor)(void *context);

//! Transform position set handler.
//! @param normalized Position in animation process (0..ANIMATION_NORMALIZED_MAX)
//! @param context User supplied context for destruction.
//! @return whether the normalized set results in an animation change
typedef bool (*TransformPositionSetter)(int32_t normalized, void *context);

//! Transform applier. The image supplied is always in its source form.
//! @param list GDrawCommandList copy of the image to be modified in-place.
//! @param size GSize of the list bounds.
//! @param from GRect pointer to the from frame of the starting position and size.
//! @param to GRect pointer to the to frame of the ending position and size.
//! @param normalized Animation position of the current transform.
//! @param context User supplied context for transform specific data.
typedef void (*TransformApply)(GDrawCommandList *list, const GSize size, const GRect *from,
                                   const GRect *to, AnimationProgress normalized, void *context);

//! Transform Implementation Callbacks.
typedef struct {
  //! Callback that is called when the kino reel is destroyed.
  TransformDestructor destructor;
  //! Callback that is called when the kino reel position is set.
  //! If no position setter is set, it is assumed that any change in position results in a change
  //! in the transform.
  TransformPositionSetter position_setter;
  //! Callback that is called when the kino reel is asked to draw.
  //! This callback is only called once for the start or end position unless the kino reel's
  //! position is changed again after reaching the start or end.
  TransformApply apply;
} TransformImpl;

//! Creates Transform Kino Reel with a custom transform implementation.
//! It is acceptable to continue to use this KinoReel after or before the animation when there
//! is no animation taking place.
//! \note This keeps in memory a copy of the image and creates an additional copy during
//! animation or at rest. At rest at a stage with a rect with a size equal to the image bounds size,
//! only a single copy is kept in memory. This is true even if arriving at the beginning stage
//! through rewinding.
//! @param impl TransformImpl pointer to a Transform implementation.
//! @param context User supplied context for transform specific data.
KinoReel *kino_reel_transform_create(const TransformImpl *impl, void *context);

//! Get the user supplied context
//! @param reel Transform Kino Reel to get a context from
void *kino_reel_transform_get_context(KinoReel *reel);

void kino_reel_transform_set_from_reel(KinoReel *reel, KinoReel *from_reel,
                                            bool take_ownership);

//! Get the from reel.
//! @param reel Transform Kino Reel to get the from reel from
KinoReel *kino_reel_transform_get_from_reel(KinoReel *reel);

void kino_reel_transform_set_to_reel(KinoReel *reel, KinoReel *to_reel,
                                          bool take_ownership);

//! Get the to reel.
//! @param reel Transform Kino Reel to get the to reel from
KinoReel *kino_reel_transform_get_to_reel(KinoReel *reel);

//! Set the layer frame. Unused if the transform was not set to be global.
//! @param reel Transform Kino Reel to set the layer frame
//! @param layer_frame Position and size of the parent KinoLayer.
void kino_reel_transform_set_layer_frame(KinoReel *reel, GRect layer_frame);

//! Set the from animation.
//! @param reel Transform Kino Reel to set the from frame
//! @param from Position and size to start from.
void kino_reel_transform_set_from_frame(KinoReel *reel, GRect from);

//! Set the to animation.
//! @param reel Transform Kino Reel to set the to frame
//! @param to Position and size to end at.
void kino_reel_transform_set_to_frame(KinoReel *reel, GRect to);

//! Get the from frame.
GRect kino_reel_transform_get_from_frame(KinoReel *reel);

//! Get the to frame.
GRect kino_reel_transform_get_to_frame(KinoReel *reel);

//! Set whether the transform takes global frames and draws globally positioned.
//! @param reel Transform Kino Reel to set whether operate in global
//! @param global Whether to draw the animation in global coordinates. If true, all frames must
//! be specified in absolute coordinates.
void kino_reel_transform_set_global(KinoReel *reel, bool global);

//! Set the duration of the transform.
//! @param reel Transform Kino Reel to set the duration of
//! @param duration Transform animation time in milliseconds.
void kino_reel_transform_set_transform_duration(KinoReel *reel, uint32_t duration);
