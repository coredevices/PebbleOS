/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Shows a frame of @p width x @p height 8-bit ARGB2222 pixels. Any thread.
void display_sdl_bottom_update(const uint8_t *fb, int width, int height);

//! Shows the colors as a watch's panel does in sunlight. Main thread.
void display_sdl_bottom_set_sunlight(bool sunlight);

//! Whether the colors are shown as in sunlight.
bool display_sdl_bottom_sunlight(void);
