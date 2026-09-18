/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! OS-owned "listening" banner shown while an app captures the microphone.
//!
//! A transparent, unfocusable modal strip at the bottom of the screen. It reserves part of the
//! app's screen through the unobstructed area service, the same way Timeline Peek does, so an app
//! can lay out around it. The app cannot cover or dismiss it: capture stops as soon as the app
//! loses focus to any other modal window.

// Round displays need a taller strip so the chord is wide enough for the centred content
#define MIC_BANNER_HEIGHT (PBL_IF_RECT_ELSE(24, 40))

void mic_banner_init(void);

//! Shows the banner. Must be called on KernelMain.
void mic_banner_show(void);

//! Hides the banner. Must be called on KernelMain.
void mic_banner_hide(void);

bool mic_banner_is_visible(void);

//! Y coordinate, in app framebuffer space, where the banner starts obstructing the app, or the
//! framebuffer height when the banner is hidden or does not overlap the app.
int16_t mic_banner_get_obstruction_origin_y(void);
