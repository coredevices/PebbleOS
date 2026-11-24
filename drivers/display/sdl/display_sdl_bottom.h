/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

//! Sets the display shown: @p width x @p height pixels, and for a round display @p mask, the
//! pixels hidden at each end of every row of its top half, mirrored in the bottom half, or
//! nullptr. Before the first frame.
void display_sdl_bottom_set_geometry(int width, int height, const uint8_t *mask);

//! Shows a frame of 8-bit ARGB2222 pixels, of the size set. Any thread.
void display_sdl_bottom_update(const uint8_t *fb);

//! Shows the colors as a watch's panel does in sunlight. Main thread.
void display_sdl_bottom_set_sunlight(bool sunlight);

//! Whether the colors are shown as in sunlight.
bool display_sdl_bottom_sunlight(void);
