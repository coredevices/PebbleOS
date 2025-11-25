/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//! @file fake_animation.h
//!
//! Simple fake of the Animation code. Not intended to be a complete drop in replacement, but good
//! enough for some simple tests.

#include "applib/ui/animation_private.h"

//! @return A pointer to the first animation that was created since we last called
//! fake_animation_cleanup
Animation * fake_animation_get_first_animation(void);

//! @return The next animation after the supplied animation. Animations form a link list based on
//! creation time, and that list can be walked by combining this function with
//! fake_animation_get_first_animation
Animation * fake_animation_get_next_animation(Animation *animation);

//! Cleans up all fake animation state. Use between tests to ensure a clean slate.
void fake_animation_cleanup(void);

//! Runs an animation to completion by scheduling it, setting its elapsed to its duration, and then
//! unscheduling it.
//! @param animation The animation to run to completion.
void fake_animation_complete(Animation *animation);
