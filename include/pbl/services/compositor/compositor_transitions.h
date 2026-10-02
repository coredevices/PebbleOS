/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "default/compositor_dot_transitions.h"
#include "default/compositor_launcher_app_transitions.h"
#include "default/compositor_slide_transitions.h"
#include "default/compositor_shutter_transitions.h"
#if CONFIG_SCREEN_COLOR_DEPTH_BITS == 1
#include "legacy/compositor_modal_slide_transitions.h"
#else
#include "default/compositor_modal_transitions.h"
#include "default/compositor_port_hole_transitions.h"
#include "default/compositor_round_flip_transitions.h"
#endif
#include "default/compositor_peek_transitions.h"

#include "applib/graphics/gdraw_command_sequence.h"

/**
 * @defgroup services_compositor_compositor_transitions Transitions
 * @ingroup services_compositor
 * @brief Ready-made compositor transitions and helpers to build new ones.
 *
 * Including this header pulls in the transitions available on the platform (see
 * @ref services_compositor_default and @ref services_compositor_legacy). Each getter returns a
 * @ref CompositorTransition to pass to compositor_transition(); app-to-app getters return NULL
 * when compositor_transition_app_to_app_should_be_skipped() says so, which switches without
 * animation.
 *
 * @code{.c}
 * const CompositorTransition *transition =
 *     compositor_shutter_transition_get(CompositorTransitionDirectionLeft, GColorRed);
 * compositor_transition(transition);
 * @endcode
 * @{
 */

/**
 * @brief Check whether app-to-app transitions should be skipped.
 *
 * @return true if an opaque modal window covers the app, so the transition would not be seen.
 */
bool compositor_transition_app_to_app_should_be_skipped(void);

/**
 * @brief Rescale animation progress to a sub-interval.
 *
 * @param time_normalized Progress to rescale, expected between @p interval_start and
 *                        @p interval_end.
 * @param interval_start Progress mapped to 0.
 * @param interval_end Progress mapped to @c ANIMATION_NORMALIZED_MAX.
 * @return Progress relative to the interval.
 */
AnimationProgress animation_timing_scaled(AnimationProgress time_normalized,
                                          AnimationProgress interval_start,
                                          AnimationProgress interval_end);

/**
 * @brief Draw the frame of a PDC sequence for the current point of a transition.
 *
 * Does nothing if @p sequence is NULL. The sequence's red strokes are drawn with
 * @p stroke_color.
 *
 * @param ctx Graphics context to draw into.
 * @param sequence PDC sequence whose frame to draw.
 * @param distance_normalized Animation progress, 0 to @c ANIMATION_NORMALIZED_MAX.
 * @param chroma_key_color Color to replace with the app framebuffer, only used when @p inner.
 * @param stroke_color Color for the stroke of the ring in the frame.
 * @param overdraw_color Color for areas of the frame without app content (e.g. flip animations
 *                       must draw the right color beyond the edges of the app face).
 * @param inner If true, draw the app framebuffer inside the ring, otherwise outside.
 * @param framebuffer_offset Visual offset of the app framebuffer. Currently ignored.
 */
void compositor_transition_pdcs_animation_update(GContext *ctx, GDrawCommandSequence *sequence,
                                                 uint32_t distance_normalized,
                                                 GColor chroma_key_color, GColor stroke_color,
                                                 GColor overdraw_color, bool inner,
                                                 const GPoint *framebuffer_offset);

/**
 * @brief Draw implementation filling lines with the contents of the app framebuffer.
 *
 * Swap it into a graphics context to make fill operations reveal the app.
 */
extern const GDrawRawImplementation g_compositor_transitions_app_fb_draw_implementation;

/** @} */
