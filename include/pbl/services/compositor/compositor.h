/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <applib/ui/property_animation.h>
#include <applib/ui/window.h>

/**
 * @defgroup services_compositor Compositor
 * @ingroup services
 * @brief Composes the app framebuffer and modal windows into the display framebuffer.
 *
 * The compositor owns the system framebuffer and flushes it to the display. It manages two
 * sources:
 *
 * - The app framebuffer, which the running app draws into. It is copied (scaled or centered when
 *   its size differs from the display) into the system framebuffer whenever the app reports a new
 *   frame with compositor_app_render_ready().
 * - The modal window stack, which is supplied by the kernel and has no framebuffer of its own. It
 *   is rendered with the kernel graphics context on top of the app (or alone, when opaque)
 *   whenever compositor_modal_render_ready() is called.
 *
 * Changes between apps or modals can be animated with a @ref CompositorTransition. All functions
 * run on KernelMain unless noted otherwise.
 *
 * @code{.c}
 * static void prv_init(Animation *animation) {
 *   animation_set_duration(animation, 6 * ANIMATION_TARGET_FRAME_INTERVAL_MS);
 * }
 *
 * static void prv_update(GContext *ctx, Animation *animation, uint32_t distance_normalized) {
 *   // Draw the frame for distance_normalized into ctx, e.g. with compositor_render_app().
 * }
 *
 * static const CompositorTransition s_my_transition = {
 *   .init = prv_init,
 *   .update = prv_update,
 * };
 *
 * compositor_transition(&s_my_transition);
 * @endcode
 * @{
 */

/**
 * @brief Transition direction, from the current position to the next.
 *
 * For example, Up is a transition to an item that lies above the current screen.
 */
typedef enum {
  /** Towards the item above. */
  CompositorTransitionDirectionUp,
  /** Towards the item below. */
  CompositorTransitionDirectionDown,
  /** Towards the item on the left. */
  CompositorTransitionDirectionLeft,
  /** Towards the item on the right. */
  CompositorTransitionDirectionRight,
  /** No particular direction. */
  CompositorTransitionDirectionNone,
} CompositorTransitionDirection;

/**
 * @brief Set up the transition animation.
 *
 * Typically sets the duration and curve, and may stash configuration in the animation context.
 *
 * @param animation Animation driving the transition.
 */
typedef void (*CompositorTransitionInitFunc)(Animation *animation);

// TODO: PBL-31460 Change compositor transitions to use AnimationProgress
// This would enable time-based bounce back transitions
/**
 * @brief Draw one frame of a transition.
 *
 * @param ctx Kernel graphics context drawing into the system framebuffer. Its draw state is
 *            restored after the call.
 * @param animation Animation driving the transition.
 * @param distance_normalized Animation progress, 0 to @c ANIMATION_NORMALIZED_MAX.
 */
typedef void (*CompositorTransitionUpdateFunc)(GContext *ctx, Animation *animation,
                                               uint32_t distance_normalized);

/**
 * @brief Clean up after a transition finished or was cancelled.
 *
 * @param animation Animation driving the transition.
 */
typedef void (*CompositorTransitionTeardownFunc)(Animation *animation);

/** @brief Implementation of a compositor transition animation. */
typedef struct CompositorTransition {
  /** Mandatory initialization function. */
  CompositorTransitionInitFunc init;
  /** Mandatory update function. */
  CompositorTransitionUpdateFunc update;
  /** Optional teardown function. */
  CompositorTransitionTeardownFunc teardown;
  /** If false, modals are rendered after each update; if true, they are skipped. */
  bool skip_modal_render_after_update;
} CompositorTransition;

/** @brief Opaque framebuffer type, see @c applib/graphics/framebuffer.h. */
typedef struct FrameBuffer FrameBuffer;

/**
 * @brief Initialize the compositor.
 *
 * Clears the system framebuffer and resets the state to showing the app.
 */
void compositor_init(void);

/**
 * @brief Start a transition.
 *
 * A transition already underway is cancelled and replaced by this one. If a display update is in
 * flight or the compositor is frozen, the start is deferred until it can render.
 *
 * For modal windows, the destination modal must already be on top of the modal window stack. For
 * apps, the destination app must be running, and the animation only begins once it has rendered
 * its first frame.
 *
 * @param impl Transition implementation, or NULL to switch without animation.
 */
void compositor_transition(const CompositorTransition *impl);

/**
 * @brief Render one transition frame with the given update function.
 *
 * The animation set up by compositor_transition() calls this automatically. Call it from an
 * animation scheduled inside a transition (e.g. one in an animation sequence) so its frames are
 * rendered, deferred and flushed like those of the transition itself.
 *
 * @param func Update function drawing the frame.
 * @param animation Animation passed to @p func.
 * @param distance_normalized Animation progress passed to @p func.
 */
void compositor_transition_render(CompositorTransitionUpdateFunc func, Animation *animation,
                                  const AnimationProgress distance_normalized);

/**
 * @brief Write the app framebuffer into the system framebuffer.
 *
 * Clears the system framebuffer to black, copies the app framebuffer (scaled or centered as
 * needed) and renders the modals on top when they are transparent. Must run on KernelMain.
 */
void compositor_render_app(void);

/**
 * @brief Render the modal window stack into the system framebuffer.
 *
 * Uses the kernel graphics context, offset by the modal transition offset.
 */
void compositor_render_modal(void);

/**
 * @brief Notify that the modal needs to redraw itself to the display.
 *
 * Ignored while a transition drives the display or a display update is in progress.
 */
void compositor_modal_render_ready(void);

/**
 * @brief Notify that the app has a new frame for the display.
 *
 * Also starts a pending app transition once the app has rendered its first frame. The app
 * framebuffer is released back to the app once copied, or when the transition completes.
 */
void compositor_app_render_ready(void);

/**
 * @brief Get the system framebuffer.
 *
 * @return System framebuffer.
 */
FrameBuffer *compositor_get_framebuffer(void);

/**
 * @brief Get the system framebuffer as a bitmap.
 *
 * @return Bitmap covering the whole system framebuffer.
 */
GBitmap compositor_get_framebuffer_as_bitmap(void);

/**
 * @brief Get the app framebuffer as a bitmap.
 *
 * The bounds are set from app_manager_get_framebuffer_size() rather than the size stored in the
 * app framebuffer, which the app could modify.
 *
 * @return Bitmap covering the app framebuffer.
 */
GBitmap compositor_get_app_framebuffer_as_bitmap(void);

/**
 * @brief Check whether a transition between apps or modals is in progress.
 *
 * @return true if a transition is running or waiting for the app's first frame.
 */
bool compositor_is_animating(void);

/**
 * @brief Set the offset at which modals are drawn during transitions that redraw them.
 *
 * @param modal_offset Offset from the display origin.
 */
void compositor_set_modal_transition_offset(GPoint modal_offset);

/** @brief Stop the current transition, if any; its teardown completes the switch. */
void compositor_transition_cancel(void);

/**
 * @brief Called once the compositor is frozen.
 *
 * @param data Data passed to compositor_freeze().
 */
typedef void (*CompositorFrozenCallback)(void *data);

/**
 * @brief Stop new frames from the app or the modal reaching the display.
 *
 * Callable from any task. @p callback runs on KernelMain once the freeze is in effect and no
 * display update is in flight, at which point the system framebuffer is stable until
 * compositor_unfreeze(). Only one freeze may be outstanding at a time.
 *
 * @param callback Called once frozen.
 * @param data Passed to @p callback.
 */
void compositor_freeze(CompositorFrozenCallback callback, void *data);

/**
 * @brief Resume pushing frames to the display, undoing compositor_freeze().
 *
 * Renders deferred while frozen run on KernelMain.
 */
void compositor_unfreeze(void);

/**
 * @brief Copy part of the app framebuffer into the system framebuffer.
 *
 * The app framebuffer content is scaled or centered in the destination as needed, based on user
 * preference. The region is clipped to the display.
 *
 * @param update_rect Region of the system framebuffer to update.
 * @param copy_relative_to_origin If false, the region is filled from the origin of the app
 *                                framebuffer; if true, from the same region of the app
 *                                framebuffer.
 */
void compositor_scaled_app_fb_copy(const GRect update_rect, bool copy_relative_to_origin);

/**
 * @brief compositor_scaled_app_fb_copy() with a vertical offset applied to the source.
 *
 * @param update_rect Region of the system framebuffer to update.
 * @param copy_relative_to_origin See compositor_scaled_app_fb_copy().
 * @param offset_y Vertical offset of the source, in pixels.
 */
void compositor_scaled_app_fb_copy_offset(const GRect update_rect, bool copy_relative_to_origin,
                                          int16_t offset_y);

/** @} */
