/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! The touchscreen is pressed at (@p x, @p y), in display coordinates, or released. Called by
//! the bottom, on the main thread.
void touch_sdl_changed(bool pressed, int x, int y);
