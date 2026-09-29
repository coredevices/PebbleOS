/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/ui/ragged_grid.h"

#include "pbl/util/math.h"

void ragged_grid_init(RaggedGrid *grid, const RaggedGridConfig *config, int selected, int total) {
  grid->config = *config;
  if (total > 0) {
    grid->anchor = ragged_grid_anchor_for(selected, total, &grid->config);
  } else {
    grid->anchor = 0;
  }
  grid->window_top = 0;
}

int ragged_grid_anchor_for(int selected, int total, const RaggedGridConfig *config) {
  if (total <= config->slim_width) {
    return 0;
  }
  // C truncates toward zero, so selected == 0 lands on step 0 and clips up.
  const int step = (selected - 1) / config->slim_width;
  const int limit = total - config->fat_width;
  int max_step = (limit - limit % config->slim_width) / config->slim_width;
  if (max_step < 1) {
    // The fat row cannot fully fit, but still lead with a slim row.
    max_step = 1;
  }
  return config->slim_width * CLIP(step, 1, max_step);
}

static int ragged_grid_rows_above(const RaggedGrid *grid) {
  return (grid->anchor + grid->config.slim_width - 1) / grid->config.slim_width;
}

int ragged_grid_row_start(const RaggedGrid *grid, int row, int total) {
  const int above = ragged_grid_rows_above(grid);
  if (row < above) {
    return grid->anchor - grid->config.slim_width * (above - row);
  }
  if (row == above) {
    return grid->anchor;
  }
  return MIN(grid->anchor + grid->config.fat_width + grid->config.slim_width * (row - above - 1),
             total);
}

int ragged_grid_row_count(const RaggedGrid *grid, int row, int total) {
  const int start = ragged_grid_row_start(grid, row, total);
  if (start >= total) {
    return 0;
  }
  if (row == ragged_grid_rows_above(grid)) {
    int end = start + grid->config.fat_width;
    if (end > total) {
      end = total;
    }
    return end - start;
  }
  int end = start + grid->config.slim_width;
  if (end > total) {
    end = total;
  }
  return end - start;
}

int ragged_grid_num_rows(const RaggedGrid *grid, int total) {
  int rows = 0;
  while (ragged_grid_row_start(grid, rows, total) < total) {
    rows++;
  }
  return rows;
}

int ragged_grid_row_for_item(const RaggedGrid *grid, int item_idx, int total) {
  int row = 0;
  while (ragged_grid_row_start(grid, row + 1, total) <= item_idx) {
    row++;
  }
  return row;
}

int ragged_grid_window_first_item(const RaggedGrid *grid, int total) {
  return ragged_grid_row_start(grid, grid->window_top, total);
}

int ragged_grid_window_last_item(const RaggedGrid *grid, int total) {
  const int rows = ragged_grid_num_rows(grid, total);
  const int last_row = MIN(grid->window_top + RAGGED_GRID_WINDOW_ROWS - 1, rows - 1);
  return ragged_grid_row_start(grid, last_row, total) +
         ragged_grid_row_count(grid, last_row, total) - 1;
}

bool ragged_grid_should_reflow(const RaggedGrid *grid, int old_idx, int new_idx, int total) {
  int old = old_idx;
  if (old < 0) {
    old = 0;
  }
  if (old >= total) {
    old = total - 1;
  }
  if (new_idx == old) {
    return false;
  }
  if (new_idx > old) {
    return old == ragged_grid_window_last_item(grid, total);
  }
  return old == ragged_grid_window_first_item(grid, total);
}

void ragged_grid_reflow(RaggedGrid *grid, int new_idx, int total) {
  grid->anchor = ragged_grid_anchor_for(new_idx, total, &grid->config);
  const int row = ragged_grid_row_for_item(grid, new_idx, total);
  // Put the selection's row on the middle window row when there is one above it.
  grid->window_top = ragged_grid_window_top_for_row(grid, row, total);
}

// Clamp a window-top so the window always shows RAGGED_GRID_WINDOW_ROWS whole
// rows: lists shorter than a window pin to 0, the rest to rows - WINDOW_ROWS.
static int ragged_grid_clamp_window_top(const RaggedGrid *grid, int top, int total) {
  const int rows = ragged_grid_num_rows(grid, total);
  // Clamp so the window is always full: a short list pins to the top.
  return CLIP(top, 0, MAX(rows - RAGGED_GRID_WINDOW_ROWS, 0));
}

int ragged_grid_window_top_for_row(const RaggedGrid *grid, int row, int total) {
  return ragged_grid_clamp_window_top(grid, row - (RAGGED_GRID_WINDOW_ROWS - 1) / 2, total);
}

void ragged_grid_sync_window_to_viewport(RaggedGrid *grid, int viewport_row, int total) {
  grid->window_top = ragged_grid_window_top_for_row(grid, viewport_row, total);
}
