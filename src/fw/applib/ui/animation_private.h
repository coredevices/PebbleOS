/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "animation.h"

#define ANIMATION_LOG_DEBUG(fmt, args...) \
            PBL_LOG_D(LOG_DOMAIN_ANIMATION, LOG_LEVEL_DEBUG, fmt, ## args)

#define ANIMATION_MAX_CHILDREN  256
#define ANIMATION_PLAY_COUNT_INFINITE_STORED ((uint16_t)~0)
#define ANIMATION_MAX_CREATE_VARGS  20

typedef enum {
  AnimationTypePrimitive,
  AnimationTypeSequence,
  AnimationTypeSpawn
} AnimationType;


//! The data structure of an animation.
typedef struct AnimationPrivate {
  //! At any one time, an animation is either in the scheduled list (scheduled_head of
  //! AnimationState) or the unscheduled list (unscheduled_head of AnimationState)
  ListNode list_node;

  //! integer handle assigned to this animation. This integer gets typecast to an
  //! (Animation *) to be used from the client's perspective.
  Animation *handle;

  const AnimationImplementation *implementation;
  AnimationHandlers handlers;
  void *context;

  //! Absolute time when the animation got scheduled, in ms since system start.
  uint32_t abs_start_time_ms;
  uint32_t delay_ms;
  uint32_t duration_ms;
  uint16_t play_count;          // Desired play count
  uint16_t times_played;        // incremented each time we play it

  AnimationCurve curve:3;
  bool is_completed:1;
  bool auto_destroy:1;
  bool being_destroyed:1;
  AnimationType type:2;
  bool is_property_animation:1; // used for cloning
  bool reverse:1;
  bool started:1;               // set true after we call the started handler
  bool calling_end_handlers:1;
  bool defer_delete:1;
  bool did_setup:1;
  bool immutable:1;

  union {
    AnimationCurveFunction custom_curve_function;
    InterpolateInt64Function custom_interpolation_function;
    void *custom_function;
  };

  // If this animation is part of a complex animation, this is the parent
  struct AnimationPrivate *parent;
  uint8_t   child_idx;    // for children of complex animations, this is the child's idx
#ifdef UNITTEST
  //! Points to the next sibling if this is a child in a complex animation and one exists
  struct AnimationPrivate *sibling;
  //! Points to the first child if this is a complex animation
  struct AnimationPrivate *first_child;
  // gets set to true when schedule() is called, false when unschedule() is called for unit tests
  bool scheduled;
#endif
} AnimationPrivate;


//! In case the 3rd party app was built for 2.0, we can't use more memory in the app state than
//! the 2.0 legacy animation does. So, we put additional context required for 3.0 into this
//! dynamically allocated block
typedef struct {
  //! Each created animation gets a unique integer handle ID
  uint32_t  next_handle;

  //! Reference to the animation that we are calling the .update handler for
  //! Will be reset to NULL once the .update handler finishes
  AnimationPrivate *current_animation;

  //! The delay the animation scheduler uses between finishing a frame and starting a new one.
  //! Derived from actual rendering/calculation times, using a PID-like control algorithm.
  uint32_t last_delay_ms;
  uint32_t last_frame_time_ms; //! Absolute time of the moment the last animation frame started.

  //! The next Animation to be iterated, NULL if at end of iteration or not iterating.
  //! This allows arbitrarily unscheduling any animation at any time.
  ListNode *iter_next;
} AnimationAuxState;


//! The currently running app task and the KernelMain task each have their own instance of
//! AnimationState which is stored as part of the app_state structure. In order to support
//! legacy 2.0 applications, this structure can be no larger than the AnimationLegacy2State
//! structure.
#define ANIMATION_STATE_3_X_SIGNATURE  ((uint32_t)(~0))
typedef struct {
  //! Signature used to distinguish these globals from the legacy 2.0 globals. The legacy 2.0
  //! globals start with a ListNode pointer. We put a value here (ANIMATION_STATE_3_X_SIGNATURE)
  //! that is guaranteed to be unique from a pointer.
  uint32_t signature;

  //! Pointer to dynamically allocated auxiliary information
  AnimationAuxState *aux;

  //! All unscheduled Animation_t's for this app appear in this list
  ListNode *unscheduled_head;

  //! All scheduled Animation_t's for this app appear in this list
  ListNode *scheduled_head;
} AnimationState;

//! Init animation state. Should be called once when task starts up
void animation_private_state_init(AnimationState *state);

//! Deinit animation state. Should be called once when task exits
void animation_private_state_deinit(AnimationState *state);

//! Init an animation structure, register it with the current task, and assign it a handle
Animation *animation_private_animation_init(AnimationPrivate *animation);

//! Return the animation object pointer for the given handle
AnimationPrivate *animation_private_animation_find(Animation *handle);

//! Timer callback triggered by the animation_service system timer
void animation_private_timer_callback(void *state);

//! Return true if the legacy2 animation manager is instantiated. State can be NULL if not already
//! known.
bool animation_private_using_legacy_2(AnimationState *state);

//! Returns the interpolation function that overrides the built-in linear interpolation,
//! or NULL if one was not set. Used to implement spatial easing.
InterpolateInt64Function animation_private_current_interpolate_override(void);

//! Returns the progress of the provided animation
AnimationProgress animation_private_get_animation_progress(const AnimationPrivate *animation);

//! Does easing and book-keeping when calling animation.implementation.update()
void animation_private_update(AnimationState *state, AnimationPrivate *animation,
                              AnimationProgress progress_raw);

//! Prevents animations from running through the animation service.
//! Any currently executing animation is not guaranteed to restart at the same frame upon resume.
//! @note this is used by test automation
void animation_private_pause(void);

//! see \ref animation_private_pause
void animation_private_resume(void);
