/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/graphics.h"

//! @file compositor_private.h
//!
//! Useful helpful function to help out implementing compositor animations

//! Trigger the app framebuffer to be copied to the system framebuffer
void compositor_render_app(void);

//! Trigger the modal window to be rendered to the system framebuffer
void compositor_render_modal(void);

//! A GPathDrawFilledCallback that can be used to fill pixels with the app's framebuffer
void compositor_app_framebuffer_fill_callback(GContext *ctx, int16_t y,
                                              Fixed_S16_3 x_range_begin, Fixed_S16_3 x_range_end,
                                              Fixed_S16_3 delta_begin, Fixed_S16_3 delta_end,
                                              void *user_data);
