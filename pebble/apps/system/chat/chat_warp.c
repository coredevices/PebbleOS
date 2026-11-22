/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "chat_warp.h"

#include <string.h>

#include <applib/app_malloc.h>
#include <applib/graphics/gcontext.h>
#include <applib/graphics/gtypes.h>

// The content is drawn a strip at a time into a rectangular scratch bitmap: a round
// framebuffer keeps no pixels outside the circle, but the rows near the bezel show content
// from far outside it.
#define STRIP_ROWS 48

typedef struct {
  uint8_t *pixels;
  int width;
  int top;  // content row of the strip's first row
} Strip;

// One channel (2 bits) of a GColor8 as 0..3.
#define CH(c, shift) (((c) >> (shift)) & 3)

static inline uint8_t prv_strip_pixel(const Strip *s, int x, int row) {
  if (x < 0 || x >= s->width || row < 0 || row >= STRIP_ROWS) {
    return 0;  // transparent
  }
  return s->pixels[row * s->width + x];
}

// Bilinear sample of the strip at (x_q8, row_q8), over the destination pixel dst: only the
// content (opaque strip pixels) is blended in, by how much of the sample it covers.
static uint8_t prv_sample(const Strip *s, int x_q8, int row_q8, uint8_t dst) {
  const int x0 = x_q8 >> 8;
  const int r0 = row_q8 >> 8;
  const int fx = x_q8 & 0xFF;
  const int fy = row_q8 & 0xFF;
  const uint8_t px[4] = {prv_strip_pixel(s, x0, r0), prv_strip_pixel(s, x0 + 1, r0),
                         prv_strip_pixel(s, x0, r0 + 1), prv_strip_pixel(s, x0 + 1, r0 + 1)};
  const int w[4] = {(256 - fx) * (256 - fy), fx * (256 - fy), (256 - fx) * fy, fx * fy};
  int cover = 0;
  int r = 0;
  int g = 0;
  int b = 0;
  for (int i = 0; i < 4; i++) {
    if (w[i] && (px[i] >> 6)) {
      cover += w[i];
      r += w[i] * CH(px[i], 4);
      g += w[i] * CH(px[i], 2);
      b += w[i] * CH(px[i], 0);
    }
  }
  if (cover == 0) {
    return dst;
  }
  // The content's colour where it covers, the background's where it doesn't (anti-aliasing)
  const int rest = 65536 - cover;
  r = (r + rest * CH(dst, 4) + 32768) >> 16;
  g = (g + rest * CH(dst, 2) + 32768) >> 16;
  b = (b + rest * CH(dst, 0) + 32768) >> 16;
  return 0xC0 | (r << 4) | (g << 2) | b;
}

static void prv_render_strip(GContext *ctx, Strip *strip, int width, uint8_t clear,
                             ChatWarpDrawFn draw, void *data) {
  memset(strip->pixels, clear, width * STRIP_ROWS);
  const GBitmap saved_bitmap = ctx->dest_bitmap;
  const GDrawState saved_state = ctx->draw_state;
  FrameBuffer *saved_framebuffer = ctx->parent_framebuffer;
  const GRect bounds = GRect(0, 0, width, STRIP_ROWS);
  ctx->dest_bitmap = (GBitmap){
    .addr = strip->pixels,
    .row_size_bytes = width,
    .info = {.format = GBitmapFormat8Bit},
    .bounds = bounds,
  };
  ctx->parent_framebuffer = nullptr;
  ctx->draw_state.clip_box = bounds;
  // Same horizontal position as on screen; the strip's first row is content row strip->top
  ctx->draw_state.drawing_box = GRect(saved_state.drawing_box.origin.x, 0,
                                      saved_state.drawing_box.size.w, STRIP_ROWS);
  draw(ctx, strip->top, data);
  ctx->dest_bitmap = saved_bitmap;
  ctx->draw_state = saved_state;
  ctx->parent_framebuffer = saved_framebuffer;
}

void chat_warp_draw(GContext *ctx, const ChatWarpMap *map, GRect region, int cx, GColor bg,
                    ChatWarpDrawFn draw, void *data) {
  // Transparent, but in the background's colour, so anti-aliased edges blend towards it
  const uint8_t clear = bg.argb & 0x3F;
  const int width = ctx->dest_bitmap.bounds.size.w;
  Strip strip = {.pixels = app_malloc(width * STRIP_ROWS), .width = width};
  if (!strip.pixels) {
    return;
  }
  const int rows = map->rows < region.size.h ? map->rows : region.size.h;
  int y = 0;
  while (y < rows) {
    // The next strip starts at the first content row this screen row needs
    strip.top = (map->src_y[y] - 128) >> 8;
    prv_render_strip(ctx, &strip, width, clear, draw, data);
    for (; y < rows; y++) {
      const int row_q8 = map->src_y[y] - 128 - strip.top * 256;
      if ((row_q8 >> 8) + 1 >= STRIP_ROWS) {
        break;  // needs rows past this strip
      }
      const int screen_y = region.origin.y + y;
      if (screen_y < 0 || screen_y >= ctx->dest_bitmap.bounds.size.h) {
        continue;
      }
      const GBitmapDataRowInfo info = gbitmap_get_data_row_info(&ctx->dest_bitmap, screen_y);
      const int x_min = info.min_x > region.origin.x ? info.min_x : region.origin.x;
      const int x_max = info.max_x < region.origin.x + region.size.w - 1
                            ? info.max_x
                            : region.origin.x + region.size.w - 1;
      const int inv = map->inv_scale[y];
      for (int x = x_min; x <= x_max; x++) {
        // The content column this pixel shows: further out from the centre, by 1 / scale
        const int x_q8 = cx * 256 + (((x * 256 + 128) - cx * 256) * inv >> 8) - 128;
        info.data[x] = prv_sample(&strip, x_q8, row_q8, info.data[x]);
      }
    }
  }
  app_free(strip.pixels);
}
