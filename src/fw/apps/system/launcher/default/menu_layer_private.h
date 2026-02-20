/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/math.h"
#include "board/display.h"

// Use display height to determine launcher cell height: larger displays use taller cells
#if PBL_DISPLAY_HEIGHT >= 200
#define LAUNCHER_MENU_LAYER_CELL_RECT_CELL_HEIGHT (53)
#else
#define LAUNCHER_MENU_LAYER_CELL_RECT_CELL_HEIGHT (42)
#endif

#if PBL_ROUND && PBL_DISPLAY_HEIGHT >= 200
#define LAUNCHER_MENU_LAYER_CELL_ROUND_FOCUSED_CELL_HEIGHT (55)
#define LAUNCHER_MENU_LAYER_CELL_ROUND_UNFOCUSED_CELL_HEIGHT (45)
#else
#define LAUNCHER_MENU_LAYER_CELL_ROUND_FOCUSED_CELL_HEIGHT (52)
#define LAUNCHER_MENU_LAYER_CELL_ROUND_UNFOCUSED_CELL_HEIGHT (38)
#endif

#if PBL_ROUND && PBL_DISPLAY_HEIGHT >= 200
//! Two "unfocused" cells above and below one centered "focused" cell (5 total for larger display)
#define LAUNCHER_MENU_LAYER_NUM_UNFOCUSED_ROWS_PER_SIDE (2)
#define LAUNCHER_MENU_LAYER_NUM_VISIBLE_ROWS (5)
#elif PBL_ROUND
//! One "unfocused" cell above and below one centered "focused" cell
#define LAUNCHER_MENU_LAYER_NUM_UNFOCUSED_ROWS_PER_SIDE (1)
#define LAUNCHER_MENU_LAYER_NUM_VISIBLE_ROWS (3)
#else
#define LAUNCHER_MENU_LAYER_NUM_VISIBLE_ROWS \
    (DIVIDE_CEIL(DISP_ROWS, LAUNCHER_MENU_LAYER_CELL_RECT_CELL_HEIGHT))
#endif
