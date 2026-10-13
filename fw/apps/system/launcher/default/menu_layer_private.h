/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/math.h>

#include <board/display.h>
#include <shell/system_theme_style.h>

#if PBL_RECT
#define LAUNCHER_MENU_LAYER_NUM_VISIBLE_ROWS \
  (DIVIDE_CEIL(DISP_ROWS, SYSTEM_THEME_LAUNCHER_MIN_CELL_HEIGHT))
#else
//! One centered "focused" cell with as many "unfocused" cells above and below as fit
#define LAUNCHER_MENU_LAYER_NUM_VISIBLE_ROWS                              \
  (1 + 2 * ((DISP_ROWS - SYSTEM_THEME_LAUNCHER_MIN_FOCUSED_CELL_HEIGHT) / \
            (2 * SYSTEM_THEME_LAUNCHER_MIN_UNFOCUSED_CELL_HEIGHT)))
#endif
