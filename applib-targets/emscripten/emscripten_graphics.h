/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "applib/graphics/gtypes.h"
#include "process_state/app_state/app_state.h"

void emx_graphics_init(void);
GContext *emx_graphics_get_gcontext(void);
void *emx_graphics_get_pixes(void);
TextRenderState *app_state_get_text_render_state(void);
void emx_graphics_call_canvas_update_proc(void);
