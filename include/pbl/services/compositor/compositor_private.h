/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <applib/graphics/graphics.h>

/**
 * @addtogroup services_compositor
 * @{
 */

void compositor_render_app(void);

void compositor_render_modal(void);

/**
 * @brief GPathDrawFilledCallback filling a span with the app framebuffer.
 *
 * Copies the matching pixels of the app framebuffer into the span, for transitions that reveal
 * the app through a path.
 *
 * @param ctx Graphics context being drawn into.
 * @param y Row of the span.
 * @param x_range_begin Start of the span.
 * @param x_range_end End of the span.
 * @param delta_begin Unused.
 * @param delta_end Unused.
 * @param user_data Optional `const GPoint *` offset of the app framebuffer, or NULL.
 */
void compositor_app_framebuffer_fill_callback(GContext *ctx, int16_t y, Fixed_S16_3 x_range_begin,
                                              Fixed_S16_3 x_range_end, Fixed_S16_3 delta_begin,
                                              Fixed_S16_3 delta_end, void *user_data);

/** @} */
