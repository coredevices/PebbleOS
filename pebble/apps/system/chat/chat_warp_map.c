/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "chat_warp.h"

typedef struct {
  int radius;
  int half_width;
  int center_q8;  // circle centre row, Q8, region coordinates
} Geometry;

static int prv_isqrt(int v) {
  if (v <= 0) {
    return 0;
  }
  int r = v;
  for (int next = (r + 1) / 2; next < r; next = (r + v / r) / 2) {
    r = next;
  }
  return r;
}

// 1 / scale (Q8) that fits the content's half width into the circle's chord at a screen row
// dist_q8 from the centre, never shrinking below CHAT_WARP_MIN_SCALE_Q8.
static int prv_fit_inv(const Geometry *g, int dist_q8) {
  if (dist_q8 < 0) {
    dist_q8 = -dist_q8;
  }
  const int r_q8 = g->radius * 256;
  const int chord_q8 = dist_q8 < r_q8 ? prv_isqrt(r_q8 * r_q8 - dist_q8 * dist_q8) : 0;
  int scale_q8 = chord_q8 / g->half_width;
  if (scale_q8 > 256) {
    scale_q8 = 256;
  }
  if (scale_q8 < CHAT_WARP_MIN_SCALE_Q8) {
    scale_q8 = CHAT_WARP_MIN_SCALE_Q8;
  }
  return 65536 / scale_q8;
}

// The item holding content row src_q8 (Q8), or -1.
static int prv_item_at(const ChatWarpItem *items, int n, int src_q8) {
  const int row = src_q8 >> 8;
  for (int i = 0; i < n; i++) {
    if (row >= items[i].top && row < items[i].bottom) {
      return i;
    }
  }
  return -1;
}

// 1 / scale for an item entered at screen row y_q8 (its near edge), whose far edge is rest
// content rows further out (dir +1 down, -1 up): small enough that the item, where it lands at
// that scale, fits the circle up to CHAT_WARP_FIT_ROWS from its near edge. Taller items would
// shrink to nothing near the bezel, so their outer rows may be cut by the circle instead.
static int prv_item_inv(const Geometry *g, int y_q8, int rest, int dir) {
  if (rest > CHAT_WARP_FIT_ROWS) {
    rest = CHAT_WARP_FIT_ROWS;
  }
  int inv = prv_fit_inv(g, y_q8 - g->center_q8);
  for (int i = 0; i < 3; i++) {
    const int far_q8 = y_q8 + dir * (rest * 65536 / inv);
    const int far_inv = prv_fit_inv(g, far_q8 - g->center_q8);
    if (far_inv <= inv) {
      break;
    }
    inv = far_inv;
  }
  return inv;
}

#define MAX_ITEMS 16

typedef struct {
  const ChatWarpItem *items;
  int n;
  int inv[MAX_ITEMS];  // each item's 1 / scale, once worked out; 0 until then
} Items;

// An item's 1 / scale, worked out where it is first met: at screen row y_q8, at content row
// src_q8, walking dir.
static int prv_item_scale(Items *it, int i, const Geometry *g, int y_q8, int src_q8, int dir) {
  if (i >= MAX_ITEMS) {
    return prv_fit_inv(g, y_q8 - g->center_q8);
  }
  if (!it->inv[i]) {
    const int row = src_q8 >> 8;
    const int rest = dir > 0 ? it->items[i].bottom - row : row - it->items[i].top;
    it->inv[i] = prv_item_inv(g, y_q8, rest, dir);
  }
  return it->inv[i];
}

// Walk from the centre row outwards (dir +1 or -1), stepping the content by each row's 1 / scale.
// A row shows its content at the scale of the item it lands in, so every row of an item matches.
static void prv_walk(ChatWarpMap *map, const Geometry *g, int c, int dir, Items *it) {
  for (int y = c + dir; y >= 0 && y < map->rows; y += dir) {
    const int y_q8 = y * 256 + 128;
    const int prev = map->src_y[y - dir];
    // Step at the scale of what the next content row belongs to
    const int next = prv_item_at(it->items, it->n, prev + dir * 256);
    const int step = next >= 0 ? prv_item_scale(it, next, g, y_q8, prev + dir * 256, dir)
                               : prv_fit_inv(g, y_q8 - g->center_q8);
    const int src = prev + dir * step;
    map->src_y[y] = src;
    const int at = prv_item_at(it->items, it->n, src);
    map->inv_scale[y] = at >= 0 ? prv_item_scale(it, at, g, y_q8, src, dir) : step;
  }
}

void chat_warp_build(ChatWarpMap *map, int rows, int center_y, int radius, int half_width,
                     const ChatWarpItem *items, int num_items) {
  if (rows > CHAT_WARP_MAX_ROWS) {
    rows = CHAT_WARP_MAX_ROWS;
  }
  map->rows = rows;
  const Geometry g = {radius, half_width, center_y * 256};
  const int c = center_y < 0 ? 0 : (center_y >= rows ? rows - 1 : center_y);
  map->src_y[c] = c * 256 + 128;

  Items it = {items, num_items, {0}};
  // An item across the centre row: one scale for both halves, fitting its farther edge
  int centre_inv = prv_fit_inv(&g, map->src_y[c] - g.center_q8);
  const int at = prv_item_at(items, num_items, map->src_y[c]);
  if (at >= 0) {
    const int up = c - items[at].top;
    const int down = items[at].bottom - c;
    // Whichever half reaches further, up to the same fit limit
    centre_inv = prv_item_inv(&g, map->src_y[c], (up > down ? up : down) / 2, 1);
    if (at < MAX_ITEMS) {
      it.inv[at] = centre_inv;
    }
  }
  map->inv_scale[c] = centre_inv;
  prv_walk(map, &g, c, 1, &it);
  prv_walk(map, &g, c, -1, &it);
}
