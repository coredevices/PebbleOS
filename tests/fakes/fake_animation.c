/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Need to use the real struct because the product code accesses the structure directly. It would
// be nice to instead create a dummy with only the fields we need, but oh well.
#include "applib/ui/animation_private.h"

#include <stdarg.h>

//! List of all animations that were created in the current test in order of creation.
ListNode *s_animations;

// Fake implementations of the real animation interface.
/////////////////////////////////////////////////////////////

Animation *animation_create(void) {
  AnimationPrivate *animation = malloc(sizeof(AnimationPrivate));
  *animation = (AnimationPrivate) {};

  if (!s_animations) {
    s_animations = (ListNode *)animation;
  } else {
    list_append(s_animations, (ListNode *)animation);
  }

  return (Animation *)animation;
}

static Animation *prv_create_from_array(Animation **animation_array, size_t array_len) {
  AnimationPrivate *parent = (AnimationPrivate *)animation_create();
  parent->first_child = (AnimationPrivate *)animation_array[0];
  for (int i = 0; i < (int)array_len; i++) {
    AnimationPrivate *child = (AnimationPrivate *)animation_array[i];
    child->parent = parent;
    if (i + 1 < (int)array_len) {
      child->sibling = (AnimationPrivate *)animation_array[i + 1];
    }
  }
  return (Animation *)parent;
}

static Animation *prv_create_from_vararg(Animation *animation_a, Animation *animation_b,
                                         Animation *animation_c, va_list args) {
  Animation *animation_array[ANIMATION_MAX_CREATE_VARGS];
  size_t array_len = 0;
  animation_array[array_len++] = animation_a;
  animation_array[array_len++] = animation_b;
  if (animation_c) {
    animation_array[array_len++] = animation_c;
    while (array_len < ANIMATION_MAX_CREATE_VARGS) {
      void *arg = va_arg(args, void *);
      if (arg == NULL) {
        break;
      }
      animation_array[array_len++] = arg;
    }
  }
  return prv_create_from_array(animation_array, array_len);
}

Animation *WEAK animation_sequence_create(Animation *animation_a, Animation *animation_b,
                                          Animation *animation_c, ...) {
  va_list args;
  va_start(args, animation_c);
  Animation *animation = prv_create_from_vararg(animation_a, animation_b, animation_c, args);
  va_end(args);
  return animation;
}

Animation *WEAK animation_sequence_create_from_array(Animation **animation_array,
                                                     uint32_t array_len) {
  return prv_create_from_array(animation_array, array_len);
}

Animation *WEAK animation_spawn_create(Animation *animation_a, Animation *animation_b,
                                       Animation *animation_c, ...) {
  va_list args;
  va_start(args, animation_c);
  Animation *animation = prv_create_from_vararg(animation_a, animation_b, animation_c, args);
  va_end(args);
  return animation;
}

Animation *WEAK animation_spawn_create_from_array(Animation **animation_array,
                                                  uint32_t array_len) {
  return prv_create_from_array(animation_array, array_len);
}

typedef void (*AnimationEachCallback)(AnimationPrivate *animation, uintptr_t context);

static void prv_each(AnimationPrivate *animation, AnimationEachCallback callback,
                     uintptr_t context) {
  if (animation->first_child) {
    prv_each(animation->first_child, callback, context);
  }
  if (animation->sibling) {
    prv_each(animation->sibling, callback, context);
  }
  callback(animation, context);
}

static void prv_free(AnimationPrivate *animation, uintptr_t context) {
  list_remove(&animation->list_node, NULL, NULL);
  free(animation);
}

bool animation_destroy(Animation *animation) {
  prv_each((AnimationPrivate *)animation, prv_free, (uintptr_t)NULL);
  return true;
}

bool animation_set_implementation(Animation *animation_h,
                                  const AnimationImplementation *implementation) {
  AnimationPrivate *animation = (AnimationPrivate *)animation_h;
  if (!animation) {
    return false;
  }
  animation->implementation = implementation;
  return true;
}

bool animation_is_scheduled(Animation *animation_h) {
  AnimationPrivate *animation = (AnimationPrivate *)animation_h;
  if (!animation) {
    return false;
  }
  return animation->scheduled;
}

uint32_t animation_get_duration(Animation *animation, bool include_delay, bool include_play_count) {
  if (!animation) {
    return 0;
  }
  return ((AnimationPrivate *)animation)->duration_ms;
}

static void prv_call_started(AnimationPrivate *animation, uintptr_t UNUSED context) {
  if (animation->implementation && animation->implementation->setup) {
    animation->implementation->setup((Animation *)animation);
  }
  if (animation->handlers.started) {
    animation->handlers.started((Animation *)animation, animation->context);
  }
}

static void prv_call_scheduled(AnimationPrivate *animation, uintptr_t scheduled) {
  animation->scheduled = scheduled;
}

bool animation_schedule(Animation *animation_h) {
  AnimationPrivate *animation = (AnimationPrivate *)animation_h;
  if (!animation) {
    return false;
  }
  if (!animation->scheduled) {
    prv_each(animation, prv_call_scheduled, true);
    // If your test is failing, build out this fake so that this is an async start
    prv_each(animation, prv_call_started, (uintptr_t)NULL);
  }
  return true;
}

bool animation_set_elapsed(Animation *animation_h, uint32_t elapsed_ms) {
  AnimationPrivate *animation = (AnimationPrivate *)animation_h;
  if (!animation) {
    return false;
  }
  if (animation->duration_ms <= elapsed_ms) {
    animation->is_completed = true;
    animation_unschedule(animation_h);
  }
  return true;
}

bool animation_get_elapsed(Animation *animation_h, int32_t *elapsed_ms) {
  AnimationPrivate *animation= (AnimationPrivate *)animation_h;
  if (!animation) {
    return false;
  }
  if (elapsed_ms) {
    *elapsed_ms = animation->is_completed ? animation->duration_ms : 0;
  }
  return true;
}

bool animation_set_handlers(Animation *animation_h, AnimationHandlers callbacks, void *context) {
  AnimationPrivate *animation = (AnimationPrivate *)animation_h;
  if (!animation) {
    return false;
  }
  animation->handlers = callbacks;
  animation->context = context;
  return true;
}


void *animation_get_context(Animation *animation_h) {
  AnimationPrivate *animation = (AnimationPrivate *)animation_h;
  if (!animation) {
    return false;
  }
  return animation->context;
}

static void prv_call_update(AnimationPrivate *animation, uintptr_t progress) {
  if (animation->implementation && animation->implementation->update) {
    animation->implementation->update((Animation *)animation, progress);
  }
}

static void prv_call_stopped(AnimationPrivate *animation, uintptr_t finished) {
  if (animation->handlers.stopped) {
    animation->handlers.stopped((Animation *)animation, finished, animation->context);
  }
  if (animation->implementation && animation->implementation->teardown) {
    animation->implementation->teardown((Animation *)animation);
  }
}

bool animation_unschedule(Animation *animation_h) {
  AnimationPrivate *animation = (AnimationPrivate *)animation_h;
  if (!animation) {
    return false;
  }
  if (animation->scheduled) {
    prv_each(animation, prv_call_scheduled, false);
    prv_each(animation, prv_call_update, ANIMATION_NORMALIZED_MAX);
    prv_each(animation, prv_call_stopped, animation->is_completed);
  }
  return true;
}

// Interface for unit tests to query the fake animation state
/////////////////////////////////////////////////////////////

Animation *fake_animation_get_first_animation(void) {
  return (Animation *)s_animations;
}

Animation *fake_animation_get_next_animation(Animation *animation) {
  return (Animation *)((AnimationPrivate *)animation)->list_node.next;
}

void fake_animation_cleanup(void) {
  ListNode *iter = s_animations;
  while (iter) {
    ListNode *current = iter;
    iter = iter->next;
    free(current);
  }

  s_animations = NULL;
}

void fake_animation_complete(Animation *animation) {
  animation_schedule(animation);
  const uint32_t duration =
      animation_get_duration(animation, false /* delay */, true /* play_count */);
  animation_set_elapsed(animation, duration);
  animation_unschedule(animation);
}
