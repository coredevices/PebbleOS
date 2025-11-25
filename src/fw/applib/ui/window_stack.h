/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "app_window_click_glue.h"
#include "window.h"
#include "window_stack_animation.h"
#include "window_stack_private.h"

#include "util/list.h"

//! @internal
//! Gets the topmost window of the given window stack.
//! @param window_stack The \ref WindowStack to obtain the topmost window for.
//! @return The topmost window of the given window stack or NULL if no window could be found.
Window *window_stack_get_top_window(WindowStack *window_stack);

//! @internal
//! Pushes a window onto the passed \ref WindowStack as the top window on
//! that stack.
//! @param window_stack The \ref WindowStack to push to
//! @param window The \ref Window to push to the top of the stack
//! @param animated Pass in `true` to animate the push using a slide animation, or
//!     `false` to skip the animation.  If this is a modal window and is the first
//!     one, it will instead use a compositor pop-to animation.
void window_stack_push(WindowStack *window_stack, Window *window, bool animated);

//! @internal
//! Like \ref window_stack_push() but with custom transitions for `push` and `pop`
//! @param window_stack The \ref WindowStack to push to
//! @param window The \ref Window to push to the top of the stack
//! @param push_transition The transition to use for when the window is pushed
//! @param pop_transition The transition to use for when the window is popped
//! @param user_data Pointer to data to add to the window's transition implementation
void window_stack_push_with_transition(WindowStack *window_stack, Window *window,
                                       const WindowTransitionImplementation *push_transition,
                                       const WindowTransitionImplementation *pop_transition);

//! Inserts a window after the top window on the passed \ref WindowStack
//! @param window_stack \ref WindowStack
//! @param window \ref Window
void window_stack_insert_next(WindowStack *window_stack, Window *window);

//! @internal
//! Pops the topmost window on the given \ref WindowStack
//! @param window_stack The \ref WindowStack to pop from
//! @param animated See \ref window_stack_remove()
//! @return The window that is popped, or NULL if there are no windows to pop
Window *window_stack_pop(WindowStack *window_stack, bool animated);

//! @internal
//! Like \ref window_stack_pop() but with a custom transition
//! @param window_stack \ref WindowStack
//! @param transition The transition to use for the pop
//! @param user_data Data to pass to the transition context
//! @return The window that is popped, or NULL if there are none.
Window *window_stack_pop_with_transition(WindowStack *window_stack,
                                         const WindowTransitionImplementation *transition);

//! Pops all windows.
//! See \ref window_stack_remove() for a description of the `animated` parameter and notes.
void window_stack_pop_all(WindowStack *window_stack, const bool animated);

//! Removes the given window from its window stack.
//! @param window The window to remove
//! @param animated Pass in `true` to animate the removal of the window using a
//!     side-to-side sliding animation to reveal the next window.  This is only
//!     used in the case the window happens to be on top of the window stack (and
//!     thus visible).  If this is a modal window and it is the last one, it will
//!     instead do a compositor pop to reveal animation.
//! @return True if window was successfully removed, otherwise False.
bool window_stack_remove(Window *window, bool animated);

//! @internal
//! Like \ref window_stack_remove() but uses the passed transition implementation.
//! @param window \ref Window
//! @param transition The transition to use for the removal
//! @param user_data Data to pass to the transition context
//! @return True if window was successfully removed, otherwise False.
bool window_stack_remove_with_transition(Window *window,
                                         const WindowTransitionImplementation *transition);

//! @internal
//! Returns whether a not the given window is on the passed window stack.
//! @param window_stack The \ref WindowStack to search
//! @param window The \ref Window to search for
//! @return True if stack contains window, otherwise False
bool window_stack_contains_window(WindowStack *window_stack, Window *window);

//! @internal
//! Counts the number of windows on the passed window stack.
//! @param window_stack The \ref WindowStack to search
//! @return The number of windows on the passed stack
uint32_t window_stack_count(WindowStack *window_stack);

//! @internal
//! Sets a flag to disallow pushing windows onto the stack. Used for popping all
//! windows off the stack when exiting apps.
//! @param window_stack The \ref WindowStack to lock
//! @see \ref window_stack_unlock_push()
void window_stack_lock_push(WindowStack *window_stack);

//! @internal
//! Unset the push lock in order to allow pushing windows onto the stack. Used
//! for popping all windows off the stack when exiting apps.
//! @param window_stack The \ref WindowStack to unlock
//! @see \ref window_stack_lock_push()
void window_stack_unlock_push(WindowStack *window_stack);

//! @internal
//! Returns a boolean indicating whether the given window stack is currently animating
//! a window or not.
//! @param window_stack The \ref WindowStack to check
//! @return boolean indicating if window stack's top window is animating
//! @see \ref window_stack_is_animating_with_fixed_status_bar() for checking if the window
//!     being animated has a fixed status bar
bool window_stack_is_animating(WindowStack *window_stack);

//! @internal
//! Like \ref window_stack_is_animating() but returns true if both \ref window_stack_is_animating()
//! would return true and the window being animated has a fixed status bar.
//! @param window_stack The \ref WindowStack to check
//! @return boolean indicating if window stack's top window is animating and has fixed status bar
//! @see \ref window_stack_is_animating()
bool window_stack_is_animating_with_fixed_status_bar(WindowStack *window_stack);
