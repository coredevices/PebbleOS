/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system/chat/chat_warp.h"

#include <clar.h>

static ChatWarpMap s_map;

void test_chat_warp__initialize(void) {
  // gabbro: a 260 px circle, the message area starting 30 px down, content 124 px each side
  chat_warp_build(&s_map, 230, 100, 130, 124, nullptr, 0);
}

void test_chat_warp__a_bubble_keeps_one_scale(void) {
  // Bubbles near the bottom: every row of each shows it at one scale, so text keeps its shape
  const ChatWarpItem items[] = {{150, 190}, {196, 240}};
  chat_warp_build(&s_map, 230, 100, 130, 124, items, 2);
  for (int i = 0; i < 2; i++) {
    int inv = -1;
    for (int y = 0; y < 230; y++) {
      const int row = s_map.src_y[y] >> 8;
      if (row >= items[i].top && row < items[i].bottom && y > 100) {
        if (inv < 0) {
          inv = s_map.inv_scale[y];
        }
        cl_assert_equal_i(s_map.inv_scale[y], inv);
      }
    }
    cl_assert(inv > 256);  // shrunk, being near the bezel
  }
}

void test_chat_warp__centre_rows_are_unchanged(void) {
  for (int y = 70; y <= 130; y++) {
    cl_assert_equal_i(s_map.inv_scale[y], 256);
    cl_assert_equal_i(s_map.src_y[y], y * 256 + 128);
  }
}

void test_chat_warp__edges_zoom_out(void) {
  // Near the bezel the content shrinks, at most by half
  cl_assert(s_map.inv_scale[0] > 256);
  cl_assert(s_map.inv_scale[229] > 256);
  cl_assert(s_map.inv_scale[229] <= 512);
  // so the region shows content from beyond its own rows: zoomed out
  cl_assert(s_map.src_y[0] < 0);
  cl_assert(s_map.src_y[229] > 230 * 256);
  // but no more than a band of extra content
  cl_assert(s_map.src_y[0] > -60 * 256);
  cl_assert(s_map.src_y[229] < 290 * 256);
}

void test_chat_warp__content_moves_steadily(void) {
  for (int y = 1; y < 230; y++) {
    const int step = s_map.src_y[y] - s_map.src_y[y - 1];
    cl_assert(step >= 256);
    cl_assert(step <= 512);
  }
}

void test_chat_warp__fits_the_circle(void) {
  // A row's content half width, scaled, stays inside the circle wherever the scale isn't
  // clamped at its minimum
  for (int y = 0; y < 230; y++) {
    if (s_map.inv_scale[y] >= 512) {
      continue;
    }
    const int d = (y - 100) * 2 + 1;  // distance from the centre, in half pixels
    const int half_q8 = 124 * 256 * 256 / s_map.inv_scale[y];
    const int chord_sq = 130 * 130 * 4 - d * d;
    cl_assert((half_q8 / 128) * (half_q8 / 128) <= chord_sq + 4 * 130);
  }
}

void test_chat_warp__every_row_of_a_bubble_matches_it(void) {
  // Small bubbles all over, as in a chat of one-line messages: no row showing a bubble may use
  // another scale than the bubble's (a row at the wrong scale shows as a notch in its edge)
  ChatWarpItem items[9];
  for (int i = 0; i < 9; i++) {
    items[i] = (ChatWarpItem){(int16_t)(-40 + i * 34), (int16_t)(-40 + i * 34 + 24)};
  }
  chat_warp_build(&s_map, 230, 100, 130, 124, items, 9);
  for (int i = 0; i < 9; i++) {
    int inv = -1;
    for (int y = 0; y < 230; y++) {
      const int row = s_map.src_y[y] >> 8;
      if (row >= items[i].top && row < items[i].bottom) {
        if (inv < 0) {
          inv = s_map.inv_scale[y];
        }
        cl_assert_equal_i(s_map.inv_scale[y], inv);
      }
    }
  }
}
