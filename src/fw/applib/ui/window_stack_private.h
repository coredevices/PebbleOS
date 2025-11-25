/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! Private API for manipulating the Window Stack
#pragma once

#include "window.h"
#include "window_stack_animation.h"
#include "util/list.h"

#include <stdbool.h>
#include <stddef.h>

#define WINDOW_STACK_ITEMS_MAX (16)

//! @internal
//! Data structure for an item on a window stack
typedef struct WindowStackItem {
  ListNode list_node;
  Window *window;
  const WindowTransitionImplementation *pop_transition_implementation;
} WindowStackItem;

//! @internal
//! Data structure for a stack of windows
typedef struct WindowStack {
  //! The item that is on top of the stack, after the last time changes were processed.
  WindowStackItem *last_top_item;

  //! The actual stack of windows. The first item in this list is the top item.
  ListNode *list_head;

  //! The list of window items that have been removed and need to be unloaded.
  ListNode *removed_list_head;

  //! Lock pushing to the stack. If this is true, pushing will not occur.
  bool lock_push;

  //! The TransitioningContext object stores the current transition being done on
  //! the window stack provided that an animation has been scheduled.
  WindowTransitioningContext transition_context;
} WindowStack;

//! Legacy handler for window transitioning.
//! @param stack The \ref WindowStack of the transitioning window.
//! @param window The \ref Window transitioning in.
bool window_transition_context_has_legacy_window_to(WindowStack *stack, Window *window);

//! Transitioning function called when the current visible window disappears.
//! If there is no window_to pointer (it is NULL), then this is a no-op.
//! @param context The \ref WindowTransitionContext of the transitioning window
void window_transition_context_disappear(WindowTransitioningContext *context);

//! Transitioning function called when the new window, window_to in the context, appears on
//! the screen.  If there is no window_to pointer (it is NULL), then this is a no-op.
//! @param context The \ref WindowTransitionContext of the transitioning window
void window_transition_context_appear(WindowTransitioningContext *context);

//! @internal
//! A member of a window stack dump array
typedef struct WindowStackDump {
  Window *addr;
  const char *name;
} WindowStackDump;

//! @internal
//! Walk and copy the window stack to an array for debug purposes.
//! @param stack The \ref WindowStack to dump.
//! @param[out] dump The windows in the stack, top of stack first.
//! @return Number of windows in the stack dump.
//!
//! The window stack dump array is allocated on the kernel heap.
//! It is the caller's responsibility to free the array.
//!
//! @note If the function is unable to allocate the dump array, *dump
//! will be set to NULL but the function will still return the depth
//! of the window stack.
size_t window_stack_dump(WindowStack *stack, WindowStackDump **dump);
