/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/util/math.h"
#include "board/display.h"

// Smallest cell heights of any content size, bounding how many rows can be visible at once
#if PBL_RECT
#define LAUNCHER_MENU_LAYER_MIN_CELL_HEIGHT (42)
#define LAUNCHER_MENU_LAYER_NUM_VISIBLE_ROWS \
  (DIVIDE_CEIL(DISP_ROWS, LAUNCHER_MENU_LAYER_MIN_CELL_HEIGHT))
#else
#define LAUNCHER_MENU_LAYER_MIN_FOCUSED_CELL_HEIGHT   (52)
#define LAUNCHER_MENU_LAYER_MIN_UNFOCUSED_CELL_HEIGHT (38)
//! One centered "focused" cell with as many "unfocused" cells above and below as fit
#define LAUNCHER_MENU_LAYER_NUM_VISIBLE_ROWS                            \
  (1 + 2 * ((DISP_ROWS - LAUNCHER_MENU_LAYER_MIN_FOCUSED_CELL_HEIGHT) / \
            (2 * LAUNCHER_MENU_LAYER_MIN_UNFOCUSED_CELL_HEIGHT)))
#endif
