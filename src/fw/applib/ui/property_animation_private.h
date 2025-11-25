/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "animation_private.h"
#include "property_animation.h"

//! The data structure of a property animation that contains all its state.
typedef struct {
  //! The "inherited" state from the "base class", \ref Animation.
  AnimationPrivate animation;
  //! The values of the property that the animation should animated from and to.
  struct {
    //! The value of the property that the animation should animate to.
    //! When the animation completes, this value will be the final value that is set.
    union {
      //! Valid when the property being animated is of type GRect
      GRect grect;
      //! Valid when the property being animated is of type GPoint
      GPoint gpoint;
      //! Valid when the property being animated is of type int16_t
      int16_t int16;
      //! Valid when the property being animated is of type GTransform
      GTransform gtransform;
      //! Valid when the property being animated is of type GColor8
      GColor8 gcolor8;
      //! Valid when the property being animated is of type Fixed_S32_16
      Fixed_S32_16 fixed_s32_16;
      //! Valid when the property being animated is of type uint32_t
      uint32_t uint32;
    } to;
    //! The value of the property that the animation should animate to.
    //! When the animation starts, this value will be the initial value that is set.
    union {
      //! Valid when the property being animated is of type GRect
      GRect grect;
      //! Valid when the property being animated is of type GPoint
      GPoint gpoint;
      //! Valid when the property being animated is of type int16_t
      int16_t int16;
      //! Valid when the property being animated is of type GTransform
      GTransform gtransform;
      //! Valid when the property being animated is of type GColor8
      GColor8 gcolor8;
      //! Valid when the property being animated is of type Fixed_S32_16
      Fixed_S32_16 fixed_s32_16;
      //! Valid when the property being animated is of type uint32_t
      uint32_t uint32;
    } from;
  } values; //! See detail table
  void *subject; //! The subject of the animation of which the property should be animated.
} PropertyAnimationPrivate;

PropertyAnimationPrivate* property_animation_private_clone(PropertyAnimationPrivate *from);
