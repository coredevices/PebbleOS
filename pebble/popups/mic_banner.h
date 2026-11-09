/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <applib/graphics/gtypes.h>

//! OS-owned "listening" indicator shown while an app uses the microphone.
//!
//! A transparent, unfocusable modal at the bottom of the screen. On round displays it is a dome
//! rising from the bottom edge. On rectangular ones a full-width "Listening..." bar slides up, then
//! after a few seconds shrinks into a box in the bottom-left corner that stays while listening.
//! It reserves the bottom of the app's screen through the unobstructed area service, the same way
//! Timeline Peek does, so an app can lay out around it. The reserved height does not change when
//! the bar shrinks; the part of the bar the box leaves free is the app's again, for its own content
//! (see mic_banner_get_accessory_area). The app cannot cover or dismiss it: the microphone stops
//! as soon as the app loses focus to any other modal window.

#if PBL_ROUND
//! Visible height of the dome
#define MIC_BANNER_HEIGHT (36)
//! Radius of the circle the dome is cut from
#define MIC_BANNER_RADIUS (82)
#endif

void mic_banner_init(void);

//! Shows the banner. Must be called on KernelMain.
void mic_banner_show(void);

//! Hides the banner. Must be called on KernelMain.
void mic_banner_hide(void);

//! Hides the banner at once, so the next app starts unobstructed. Must be called on KernelMain.
void mic_banner_handle_process_kill(void);

bool mic_banner_is_visible(void);

//! Y coordinate, in app framebuffer space, where the banner starts obstructing the app, or the
//! framebuffer height when the banner is hidden or does not overlap the app.
int16_t mic_banner_get_obstruction_origin_y(void);

//! The part of the bar that the corner box leaves to the app, in app framebuffer space. The full
//! bar covers it at first and uncovers it as it shrinks. It keeps the bar's height while the banner
//! slides, so what the app draws in it moves with the banner. True from when an app takes the mic
//! (the area is then just below the screen) until the banner has slid away. False on round
//! displays, where the dome leaves no room, and when the banner does not overlap the app.
bool mic_banner_get_accessory_area(GRect *area_out);
