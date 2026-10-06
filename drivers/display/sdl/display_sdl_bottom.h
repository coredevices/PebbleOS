/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

//! Shows a frame of @p width x @p height 8-bit ARGB2222 pixels. Any thread.
void display_sdl_bottom_update(const uint8_t *fb, int width, int height);
