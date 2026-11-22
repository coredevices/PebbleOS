/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */
//! Round screens: the chat is drawn as a wide flat column and squeezed into the circle, like
//! Wear OS's scaling lists. Rows near the top and bottom of the screen show the content shrunk
//! (in both directions) around the screen's centre, so text approaching the bezel zooms out
//! instead of running into it, and zooms back in as it scrolls towards the middle.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define CHAT_WARP_MAX_ROWS 260
//! Smallest scale, at the very top and bottom
#define CHAT_WARP_MIN_SCALE_Q8 128
//! Gap between the message column and the screen's sides, before the warp
#define CHAT_WARP_SIDE_INSET 14
//! Gap kept between squeezed content and the bezel
#define CHAT_WARP_BEZEL_GAP 8
//! How far into an item (rows, about three lines) its scale must fit the circle
#define CHAT_WARP_FIT_ROWS 56

typedef struct {
  int rows;
  //! For each screen row of the region, the content row it shows (its centre), Q8, in region
  //! coordinates: below 0 or past rows is content scrolled off the region.
  int32_t src_y[CHAT_WARP_MAX_ROWS];
  //! For each row, 1 / scale, Q8 (256: no squeeze).
  uint16_t inv_scale[CHAT_WARP_MAX_ROWS];
} ChatWarpMap;

//! Content rows [top, bottom) (region coordinates) scaled as one: a message bubble.
typedef struct {
  int16_t top;
  int16_t bottom;
} ChatWarpItem;

//! Map the rows [0, rows) of a region whose circle centre is at center_y (region coordinates),
//! for content half_width wide on each side of the centre, in a circle of the given radius.
//! Each item gets one scale, small enough for its far edge to fit the circle, so its text keeps
//! its shape; rows between items follow the circle. items are sorted by top.
void chat_warp_build(ChatWarpMap *map, int rows, int center_y, int radius, int half_width,
                     const ChatWarpItem *items, int num_items);

#if !UNITTEST
#include <applib/graphics/gtypes.h>

//! Draws the content rows starting at content row y0 of the region (region coordinates; may be
//! negative or past the region) into ctx, whose clip is a strip of rows starting there.
typedef void (*ChatWarpDrawFn)(GContext *ctx, int y0, void *data);

//! Draw the content through the warp onto ctx's framebuffer, in region (screen coordinates),
//! over what is already there: only pixels the content covers are written. cx is the circle's
//! centre column on screen; bg the colour the content is drawn over (for anti-aliasing).
void chat_warp_draw(GContext *ctx, const ChatWarpMap *map, GRect region, int cx, GColor bg,
                    ChatWarpDrawFn draw, void *data);
#endif
