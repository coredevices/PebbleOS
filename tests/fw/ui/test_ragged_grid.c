/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/ragged_grid.h"

#include "clar.h"

static const RaggedGridConfig s_config_3x4 = {
  .fat_width = 4,
  .slim_width = 3,
};

static RaggedGrid prv_grid(int anchor, int top) {
  RaggedGrid grid;
  grid.config = s_config_3x4;
  grid.anchor = anchor;
  grid.window_top = top;
  return grid;
}

void test_ragged_grid__init(void) {
  RaggedGrid grid;
  ragged_grid_init(&grid, &s_config_3x4, 0, 21);
  cl_check(grid.anchor == 3);
  cl_check(grid.window_top == 0);

  // Short lists still lead with a slim row when one fits.
  ragged_grid_init(&grid, &s_config_3x4, 4, 5);
  cl_check(grid.anchor == 3);
  cl_check(grid.window_top == 0);

  ragged_grid_init(&grid, &s_config_3x4, 0, 0);
  cl_check(grid.anchor == 0);
}

void test_ragged_grid__anchor_for(void) {
  cl_check(ragged_grid_anchor_for(0, 21, &s_config_3x4) == 3);
  cl_check(ragged_grid_anchor_for(9, 21, &s_config_3x4) == 6);
  cl_check(ragged_grid_anchor_for(10, 21, &s_config_3x4) == 9);
  cl_check(ragged_grid_anchor_for(14, 21, &s_config_3x4) == 12);
  cl_check(ragged_grid_anchor_for(19, 21, &s_config_3x4) == 15);
  cl_check(ragged_grid_anchor_for(20, 21, &s_config_3x4) == 15);
  // Lists too short for a full fat row still lead with a slim row.
  cl_check(ragged_grid_anchor_for(4, 5, &s_config_3x4) == 3);
  cl_check(ragged_grid_anchor_for(0, 4, &s_config_3x4) == 3);
  cl_check(ragged_grid_anchor_for(5, 6, &s_config_3x4) == 3);
  // At most slim_width items: a single row, anchor irrelevant.
  cl_check(ragged_grid_anchor_for(0, 2, &s_config_3x4) == 0);
  cl_check(ragged_grid_anchor_for(3, 3, &s_config_3x4) == 0);
}

void test_ragged_grid__partition(void) {
  RaggedGrid grid = prv_grid(3, 0);
  cl_check(ragged_grid_num_rows(&grid, 21) == 7);
  // Row 0: lead 3-cell row.
  cl_check(ragged_grid_row_start(&grid, 0, 21) == 0);
  cl_check(ragged_grid_row_count(&grid, 0, 21) == 3);
  // Row 1: fat row.
  cl_check(ragged_grid_row_start(&grid, 1, 21) == 3);
  cl_check(ragged_grid_row_count(&grid, 1, 21) == 4);
  // Row 6: two-cell tail.
  cl_check(ragged_grid_row_start(&grid, 6, 21) == 19);
  cl_check(ragged_grid_row_count(&grid, 6, 21) == 2);
  // Past the end.
  cl_check(ragged_grid_row_start(&grid, 7, 21) == 21);
  cl_check(ragged_grid_row_count(&grid, 7, 21) == 0);
}

void test_ragged_grid__row_for_item(void) {
  RaggedGrid grid = prv_grid(3, 0);
  cl_check(ragged_grid_row_for_item(&grid, 0, 21) == 0);
  cl_check(ragged_grid_row_for_item(&grid, 2, 21) == 0);
  cl_check(ragged_grid_row_for_item(&grid, 3, 21) == 1);
  cl_check(ragged_grid_row_for_item(&grid, 6, 21) == 1);
  cl_check(ragged_grid_row_for_item(&grid, 7, 21) == 2);
  cl_check(ragged_grid_row_for_item(&grid, 20, 21) == 6);
}

void test_ragged_grid__tiny_lists(void) {
  // Reachable state for a five-item list: slim lead row, clamped fat row.
  RaggedGrid grid = prv_grid(3, 0);
  cl_check(ragged_grid_num_rows(&grid, 5) == 2);
  cl_check(ragged_grid_row_start(&grid, 0, 5) == 0);
  cl_check(ragged_grid_row_count(&grid, 0, 5) == 3);
  cl_check(ragged_grid_row_start(&grid, 1, 5) == 3);
  cl_check(ragged_grid_row_count(&grid, 1, 5) == 2);
  // Single-row lists (anchor 0).
  RaggedGrid tiny = prv_grid(0, 0);
  cl_check(ragged_grid_num_rows(&tiny, 2) == 1);
  cl_check(ragged_grid_row_start(&tiny, 0, 2) == 0);
  cl_check(ragged_grid_row_count(&tiny, 0, 2) == 2);
  cl_check(ragged_grid_num_rows(&tiny, 0) == 0);
}

void test_ragged_grid__window_edges(void) {
  RaggedGrid grid = prv_grid(3, 0);
  cl_check(ragged_grid_window_first_item(&grid, 21) == 0);
  cl_check(ragged_grid_window_last_item(&grid, 21) == 9);

  RaggedGrid scrolled = prv_grid(12, 4);
  cl_check(ragged_grid_window_first_item(&scrolled, 21) == 12);
  cl_check(ragged_grid_window_last_item(&scrolled, 21) == 20);
}

void test_ragged_grid__reflow_triggers(void) {
  RaggedGrid grid = prv_grid(3, 0);
  // Roaming inside the window never reflows.
  cl_check(!ragged_grid_should_reflow(&grid, 0, 1, 21));
  cl_check(!ragged_grid_should_reflow(&grid, 8, 9, 21));
  cl_check(!ragged_grid_should_reflow(&grid, 10, 9, 21));
  // Down off the last visible cell reflows.
  cl_check(ragged_grid_should_reflow(&grid, 9, 10, 21));
  // Re-asserting the same selection never reflows (e.g. cache rebuild or a
  // scroll that clipped at the list edge).
  cl_check(!ragged_grid_should_reflow(&grid, 0, 0, 21));
  cl_check(!ragged_grid_should_reflow(&grid, 9, 9, 21));
  // Entering the grid from a main-item selection (old index negative, clipped
  // to the first item) must not reflow either.
  cl_check(!ragged_grid_should_reflow(&grid, -1, 0, 21));

  // Up off the first visible cell reflows (window rows 4,5 here).
  RaggedGrid scrolled = prv_grid(12, 4);
  cl_check(ragged_grid_should_reflow(&scrolled, 12, 11, 21));
  cl_check(!ragged_grid_should_reflow(&scrolled, 20, 19, 21));
}

void test_ragged_grid__reflow_updates_state(void) {
  RaggedGrid grid = prv_grid(3, 0);
  ragged_grid_reflow(&grid, 10, 21);
  cl_check(grid.anchor == 9);
  // Selection row recentered: row of 10 is 3, top becomes 2.
  cl_check(grid.window_top == 2);
  cl_check(ragged_grid_window_first_item(&grid, 21) == 6);
  cl_check(ragged_grid_window_last_item(&grid, 21) == 15);
}

void test_ragged_grid__window_top_for_row(void) {
  RaggedGrid grid = prv_grid(3, 0);
  // Rows 0..2 pin window_top 0..1 with a 3-row window (no room to center).
  cl_check(ragged_grid_window_top_for_row(&grid, 0, 21) == 0);
  cl_check(ragged_grid_window_top_for_row(&grid, 1, 21) == 0);
  cl_check(ragged_grid_window_top_for_row(&grid, 2, 21) == 1);
  // Centering in the middle of the list.
  cl_check(ragged_grid_window_top_for_row(&grid, 3, 21) == 2);
  // Clamped at the tail: last row never scrolls past it.
  cl_check(ragged_grid_window_top_for_row(&grid, 6, 21) == 4);
  cl_check(ragged_grid_window_top_for_row(&grid, 9, 21) == 4);
}

void test_ragged_grid__sync_window_to_viewport(void) {
  // Re-derives the window from a viewport-rest row (the middle visible row).
  RaggedGrid grid = prv_grid(3, 0);
  ragged_grid_sync_window_to_viewport(&grid, 1, 21);
  cl_check(grid.window_top == 0);
  ragged_grid_sync_window_to_viewport(&grid, 3, 21);
  cl_check(grid.window_top == 2);
  // A viewport resting beyond the tail clamps back into range.
  ragged_grid_sync_window_to_viewport(&grid, 6, 21);
  cl_check(grid.window_top == 4);
}

// Recenters the roam window on a row, as the production roam path does.
static void prv_recenter_window(RaggedGrid *grid, int row) {
  if (row < grid->window_top) {
    grid->window_top = row;
  }
  if (row > grid->window_top + (RAGGED_GRID_WINDOW_ROWS - 1)) {
    grid->window_top = row - (RAGGED_GRID_WINDOW_ROWS - 1);
  }
}

// Walks from *sel towards the list edge in direction delta, applying the
// production roam/reflow protocol. Each reflow must land on the expected
// window edge from edges[]. Returns the number of reflows observed.
static int prv_walk(RaggedGrid *grid, int *sel, int total, int delta, bool *visited,
                    const int (*edges)[2], int num_edges) {
  int reflows = 0;
  while (*sel + delta >= 0 && *sel + delta < total) {
    const int next = *sel + delta;
    if (ragged_grid_should_reflow(grid, *sel, next, total)) {
      cl_check(reflows < num_edges);
      cl_check(*sel == edges[reflows][0]);
      cl_check(next == edges[reflows][1]);
      reflows++;
      ragged_grid_reflow(grid, next, total);
    } else {
      prv_recenter_window(grid, ragged_grid_row_for_item(grid, next, total));
    }
    *sel = next;
    visited[*sel] = true;
  }
  return reflows;
}

// Full down-then-up walk through the real module calls: page turns happen
// exactly on window-edge pushes, every cell is visited both ways, and both
// ends settle back to stable states.
void test_ragged_grid__full_sweep(void) {
  RaggedGrid grid;
  ragged_grid_init(&grid, &s_config_3x4, 0, 21);
  int sel = 0;
  bool visited[21] = {false};
  visited[0] = true;
  static const int down_edges[][2] = {{9, 10}, {15, 16}};
  static const int up_edges[][2] = {{12, 11}, {6, 5}};

  const int down_reflows = prv_walk(&grid, &sel, 21, 1, visited, down_edges, 2);
  cl_check(sel == 20);
  cl_check(down_reflows == 2);
  cl_check(grid.anchor == 15);
  cl_check(grid.window_top == 4);
  for (int i = 0; i < 21; i++) {
    cl_check(visited[i]);
  }

  const int up_reflows = prv_walk(&grid, &sel, 21, -1, visited, up_edges, 2);
  cl_check(sel == 0);
  cl_check(up_reflows == 2);
  cl_check(grid.anchor == 3);
  cl_check(grid.window_top == 0);
}
