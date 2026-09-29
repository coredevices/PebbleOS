/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>

//! @file ragged_grid.h
//! @addtogroup UI
//! @{
//!   @addtogroup RaggedGrid
//!   @{
//!
//!   Row layout for grids whose rows hold different item counts (e.g. a
//!   circular watch face fitting an extra column in its wide middle rows).
//!
//!   The layout is a pure function of item count plus a frozen anchor: the
//!   item index where the fat row starts. Rows above the anchor hold
//!   slim_width items (top row partial), the anchor row holds fat_width,
//!   rows below hold slim_width (tail partial). Callers roam the cursor
//!   inside the frozen layout and only move the anchor on page turns, so
//!   items never reshuffle under the cursor.
//!
//!   All functions are pure index math with no UI dependencies. Callers
//!   must @ref ragged_grid_init "initialize" a grid before any query.
//!   @} // end addtogroup RaggedGrid
//! @} // end addtogroup UI

//! Visible window height, in rows. Callers roam the cursor inside a window
//! of this many rows; leaving the first or last visible row is what triggers
//! a reflow.
#define RAGGED_GRID_WINDOW_ROWS 3

//! Row widths. Lists of at most slim_width items form a single row; longer
//! lists lead with a slim row so the fat row sits below it.
typedef struct {
  int fat_width;
  int slim_width;
} RaggedGridConfig;

//! Frozen layout state: fat-row start plus visible-window top row.
typedef struct {
  RaggedGridConfig config;
  int anchor;
  int window_top;
} RaggedGrid;

//! Reset the layout for a new item list. Safe for any selected/total.
void ragged_grid_init(RaggedGrid *grid, const RaggedGridConfig *config, int selected, int total);

//! Fat-row start for a selection: a multiple of slim_width, clamped so the
//! fat row fits when possible. Lists of at most slim_width items pin to 0.
int ragged_grid_anchor_for(int selected, int total, const RaggedGridConfig *config);

//! First item index of a row; equals total when past the end.
//! Requires 0 <= row and total >= 0.
int ragged_grid_row_start(const RaggedGrid *grid, int row, int total);

//! Item count of a row. Requires a valid row (start < total).
int ragged_grid_row_count(const RaggedGrid *grid, int row, int total);

//! Row count of the whole list.
int ragged_grid_num_rows(const RaggedGrid *grid, int total);

//! Row holding an item index. Requires 0 <= item_idx < total.
int ragged_grid_row_for_item(const RaggedGrid *grid, int item_idx, int total);

//! First item of the visible window. Requires a valid window_top.
int ragged_grid_window_first_item(const RaggedGrid *grid, int total);

//! Last item of the visible window. Requires total >= 1 and a valid window_top.
int ragged_grid_window_last_item(const RaggedGrid *grid, int total);

//! True when stepping to new_idx pushes past the visible window edge, which
//! is the only trigger for a reflow. Stepping to the same index (or from a
//! negative index clipped to the first item) never reflows. Requires
//! total >= 1.
bool ragged_grid_should_reflow(const RaggedGrid *grid, int old_idx, int new_idx, int total);

//! Move the anchor to a selection and recenter the window on its row.
//! Requires 0 <= new_idx < total.
void ragged_grid_reflow(RaggedGrid *grid, int new_idx, int total);

//! Window-top value that centers \a row in the visible window, clamped to the
//! list. Pure; shared by reflow centering and window re-centering.
int ragged_grid_window_top_for_row(const RaggedGrid *grid, int row, int total);

//! Re-derive the window from the row actually centered in the on-screen
//! viewport, after the viewport moved independently of the window. Requires
//! 0 <= viewport_row.
void ragged_grid_sync_window_to_viewport(RaggedGrid *grid, int viewport_row, int total);
