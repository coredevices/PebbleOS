/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/math.h>

#include <applib/platform.h>
#include <shell/system_theme_style.h>

#if PBL_RECT
#define LAUNCHER_MENU_LAYER_NUM_VISIBLE_ROWS \
  (DIVIDE_CEIL(PBL_DISPLAY_HEIGHT, SYSTEM_THEME_LAUNCHER_MIN_CELL_HEIGHT))
#else
//! One centered "focused" cell with as many "unfocused" cells above and below as fit
#define LAUNCHER_MENU_LAYER_NUM_VISIBLE_ROWS                                       \
  (1 + 2 * ((PBL_DISPLAY_HEIGHT - SYSTEM_THEME_LAUNCHER_MIN_FOCUSED_CELL_HEIGHT) / \
            (2 * SYSTEM_THEME_LAUNCHER_MIN_UNFOCUSED_CELL_HEIGHT)))
#endif
