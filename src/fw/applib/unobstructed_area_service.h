/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "applib/ui/animation.h"

//! @addtogroup UI
//! @{
//!   @addtogroup UnobstructedArea
//!
//! \brief Events about when the app's unobstructed area changes for visually adapting
//!
//! Unobstructed Area enables a watchface to adapt to overlays partially obstructing it. Timeline
//! Peek is the only overlay, and it partially obstructs the bottom of watchfaces, displaying the
//! immediately upcoming or newly began event. Unobstructed Area is for use only with watchfaces.
//! There will be no Unobstructed Area events emitted for apps that are not watchfaces. Timeline
//! Peek is also limited to rectangular platforms, thus using Unobstructed Area on Chalk will also
//! result in no events.
//!
//! Watchfaces are encouraged to respect Unobstructed Area in order to dynamically resize within
//! the remaining screen area that isn't obstructed by an overlay. Unobstructed Area provides
//! handlers for when the screen area will begin and end changing as well as progress for every
//! frame of the animation, allowing one to animate the watchface to resize while the overlay
//! animates into or out of the screen. Additionally, at any point, a watchface can query the
//! Unobstructed Area using \ref layer_get_unobstructed_bounds even if there was not event or if it
//! didn't subscribe to them.
//!
//! The simplest usage of Unobstructed Area is to change rendering of your watchface to render
//! within the unobstructed bounds obtained by \ref layer_get_unobstructed_bounds. This is
//! accomplished by modifying your layer update procedure that you setup a layer with using
//! \ref layer_set_update_proc to use \ref layer_get_unobstructed_bounds instead of
//! \ref layer_get_bounds or \ref layer_get_frame and adjusting all the rendering logic within it
//! to handle resizing bounds. The app will redraw automatically whenever the Unobstructed
//! Area changes, even if you did not subscribe to the events.
//!
//! Subscribing to `will_change`, `change`, and `did_change` events with
//! \ref app_unobstructed_area_service_subscribe will allow you to obtain the final unobstructed
//! area as soon as the animation begins and the current animation progress for each frame as they
//! occur should you need that information to resize your watchface.
//!
//! When designing your watchface resizing logic or animation, take into consideration the way
//! Timeline Peek animates. Timeline Peek appears from the bottom with an animation that moves
//! slightly at first and snaps across with a bounce back. Because there is a bounce back in the
//! animation, there will be momentary frames where the unobstructed area is slightly smaller than
//! when Timeline Peek is visible but not animating.
//!
//! If you intend to hide or show layers on the screen depending on the unobstructed area, there
//! are two ways to do this. For anything inside a layer update procedure the simple solution would
//! be sufficient. That is, in your update procedure, you can to determine whether something is to
//! be rendered or not based on the remaining height available to you determined from
//! \ref layer_get_unobstructed_bounds.
//!
//! The other method for deciding whether to show or hide layers is to subscribe to events using
//! \ref app_unobstructed_area_service_subscribe. Note that hiding or showing elements in your
//! `will_change` can be too early due to the nature of the Timeline Peek animation where it moves
//! slowly in the first few frames. After judging the animation visually, you may instead show or
//! hide your layers based on both the final unobstructed area obtained from `will_change` in
//! combination with the progress obtained from `change`, or check the current height available to
//! you by calling \ref layer_get_unobstructed_bounds in your `change` handler to update your
//! state.
//!
//! For more advance usages such as functionally defined animations whose animation frames are a
//! function of animation progress, you will want to subscribe to `will_change`, `change`, and
//! optionally `did_change` events if you need to clean up resources or layers created in your
//! `will_change` handler. For example, in your `will_change` handler, you may use
//! \ref layer_get_unobstructed_bounds to get the current unobstructed area, and combine it with
//! the final unobstructed area passed into `will_change` to set up a \ref PropertyAnimation. A
//! separate example would be to save the animation progress passed to your `change` handler for
//! use in your own custom easing curve in your layer update procedure.

//! Handler that will be called just before the unobstructed area will begin changing
//! @param final_unobstructed_screen_area The final unobstructed screen area after
//! the unobstructed area has finished changing.
//! @param context A user-provided context.
typedef void (*UnobstructedAreaWillChangeHandler)(GRect final_unobstructed_screen_area,
                                                  void *context);

//! Handler that will be called every time the unobstructed area changes
//! @param progress The progress of the animation changing the unobstructed area.
//! @param context A user-provided context.
typedef void (*UnobstructedAreaChangeHandler)(AnimationProgress progress, void *context);

//! Handler that will be called after the unobstructed area has finished changing
//! @param context A user-provided context.
typedef void (*UnobstructedAreaDidChangeHandler)(void *context);

typedef struct UnobstructedAreaHandlers {
  //! Handler that will be called just before the unobstructed area will begin changing.
  UnobstructedAreaWillChangeHandler will_change;
  //! Handler that will be called every time the unobstructed area changes.
  UnobstructedAreaChangeHandler change;
  //! Handler that will be called after the unobstructed area has finished changing.
  UnobstructedAreaDidChangeHandler did_change;
} UnobstructedAreaHandlers;

//! @internal
//! Puts a will change event notifying of the start of an obstruction animation.
//! @param current_y The beginning obstruction y which is saved in the UnobstructedAreaState
//! @param final_y The final obstruction y which is converted to the unobstructed area and passed
//! to the `will_change` handler
void unobstructed_area_service_will_change(int16_t current_y, int16_t final_y);

//! @internal
//! Puts a change event notifying a frame change of an obstruction animation.
//! If the app was newly started, this will also put a will change event.
//! @param current_y The current obstruction y to be saved in UnobstructedAreaState for this frame
//! @param final_y The final obstruction y which is converted to the unobstructed area and may be
//! passed to the `will_change` handler if the app did not yet receive a will change event due to
//! being started mid-animation.
void unobstructed_area_service_change(int16_t current_y, int16_t final_y,
                                      AnimationProgress progress);
//! @internal
//! Puts a did change event notifying that the obstruction animation is complete.
//! @param final_y The final obstruction y which is converted to the unobstructed area and may be
//! passed to the `will_change` handler if the app did not yet receive a will change event due to
//! being started mid-animation.
void unobstructed_area_service_did_change(int16_t final_y);

//! Subscribe to be notified when the app's unobstructed area changes. When an unobstructed area
//! begins changing, the `will_change` handler will be called, and every `will_change` call is
//! always paired with a `did_change` call that occurs when it is done changing given that
//! the `will_change` and `did_change` handlers are set. When subscribing while the unobstructed
//! area is changing, the `will_change` handler will be called after subscription in the next event
//! loop.
//! @param handlers The handlers that should be called when the unobstructed area changes.
//! @param context A user-provided context that will be passed to the callback handlers.
//! @see layer_get_unobstructed_bounds
void app_unobstructed_area_service_subscribe(UnobstructedAreaHandlers handlers, void *context);

//! Unsubscribe from notifications about changes to the app's unobstructed area.
void app_unobstructed_area_service_unsubscribe(void);

//!   @} // end addtogroup UnobstructedArea
//! @} // end addtogroup UI
