/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include "applib/graphics/gtypes.h"

bool sdl_graphics_init(void);
void sdl_graphics_render(void);
GContext *sdl_graphics_get_gcontext(void);
