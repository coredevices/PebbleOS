/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/analytics/analytics.h>
#include <pbl/drivers/display.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/irq.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/time.h>
#include <pbl/services/compositor/compositor.h>
#include <pbl/services/compositor/compositor_private.h>
#include <pbl/util/math.h>

#include <applib/graphics/framebuffer.h>
#include <applib/graphics/gcolor_definitions.h>
#include <applib/graphics/gtypes.h>
#include <kernel/events.h>

static void (*s_update_complete_handler)(bool success);

static uint64_t s_flush_start_ticks;
static uint32_t s_flush_count;
static uint64_t s_flush_ticks;

static uint16_t s_flush_y0;
static uint16_t s_flush_y1;

// Corner pixels hidden by the display mounting, drawn into the framebuffer for the flush and
// undone after it. Holds the bytes of each masked row: [row][left, right][byte]
static uint8_t *s_saved_corners;
static uint8_t s_corner_bytes;

#if PBL_BW
#define PIXEL_BYTE(x) ((x) / 8)
#else
#define PIXEL_BYTE(x) (x)
#endif

static void prv_mask_pixel(uint8_t *line, uint16_t x) {
#if PBL_BW
  line[x / 8] &= ~(1U << (x % 8));
#else
  line[x] = GColorBlackARGB8;
#endif
}

static uint8_t prv_corner_row(uint16_t y, uint8_t *save_idx) {
  const uint8_t rows = DISPLAY->num_corners;

  if (y < rows) {
    *save_idx = y;
    return DISPLAY->corners[y];
  }

  if (y >= PBL_DISPLAY_HEIGHT - rows) {
    *save_idx = rows + (PBL_DISPLAY_HEIGHT - y - 1);
    return DISPLAY->corners[PBL_DISPLAY_HEIGHT - y - 1];
  }

  return 0;
}

static void prv_corners_apply(FrameBuffer *fb, bool mask) {
  if (s_saved_corners == nullptr) {
    return;
  }

  for (uint16_t y = s_flush_y0; y <= s_flush_y1; y++) {
    uint8_t idx;
    const uint8_t width = prv_corner_row(y, &idx);
    if (width == 0) {
      continue;
    }

    uint8_t *line = (uint8_t *)framebuffer_get_line(fb, y);
    uint8_t *saved = &s_saved_corners[idx * 2 * s_corner_bytes];
    uint16_t left = PIXEL_BYTE(0);
    uint16_t right = PIXEL_BYTE(PBL_DISPLAY_WIDTH - width);
    uint16_t len = PIXEL_BYTE(width - 1) - left + 1;

    if (mask) {
      memcpy(saved, &line[left], len);
      memcpy(&saved[s_corner_bytes], &line[right], len);
      for (uint8_t x = 0; x < width; x++) {
        prv_mask_pixel(line, x);
        prv_mask_pixel(line, PBL_DISPLAY_WIDTH - x - 1);
      }
    } else {
      memcpy(&line[left], saved, len);
      memcpy(&line[right], &saved[s_corner_bytes], len);
    }
  }
}

static void prv_flush_finish(void *data) {
  bool success = (data == nullptr);

  prv_corners_apply(compositor_get_framebuffer(), false);

  uint64_t elapsed = pbl_uptime_ticks() - s_flush_start_ticks;
  pbl_irq_lock();
  s_flush_count++;
  s_flush_ticks += elapsed;
  pbl_irq_unlock();

  framebuffer_reset_dirty(compositor_get_framebuffer());

  if (s_update_complete_handler) {
    s_update_complete_handler(success);
  }
}

// The display reports from its interrupt, or synchronously from compositor_display_update()
static void prv_flush_done(int status, void *arg) {
  void *data = (status == 0) ? nullptr : (void *)1;

  if (!pbl_in_isr()) {
    prv_flush_finish(data);
    return;
  }

  PebbleEvent e = {
    .type = PEBBLE_CALLBACK_EVENT,
    .callback = {
      .callback = prv_flush_finish,
      .data = data,
    },
  };

  event_put_isr(&e);
}

void compositor_display_init(void) {
#if PBL_BW
  const enum pbl_display_format format = PBL_DISPLAY_FORMAT_MONO;
#else
  const enum pbl_display_format format = PBL_DISPLAY_FORMAT_ARGB2222;
#endif

  PBL_ASSERTN((DISPLAY->width == PBL_DISPLAY_WIDTH) && (DISPLAY->height == PBL_DISPLAY_HEIGHT) &&
              (DISPLAY->format == format));
#if PBL_ROUND
  PBL_ASSERTN(DISPLAY->round_mask != nullptr);
#endif

  uint8_t max_width = 0;
  for (uint8_t i = 0; i < DISPLAY->num_corners; i++) {
    max_width = MAX(max_width, DISPLAY->corners[i]);
  }

  if (max_width > 0) {
    s_corner_bytes = PIXEL_BYTE(max_width - 1) + 1;
    s_saved_corners = pbl_malloc_check(DISPLAY->num_corners * 2 * 2 * s_corner_bytes);
  }
}

void compositor_display_update(void (*handle_update_complete_cb)(bool success)) {
  FrameBuffer *fb = compositor_get_framebuffer();
  if (!framebuffer_is_dirty(fb)) {
    return;
  }
  // Every flush drops the mirror; the compositor restores it after an app frame's flush
  compositor_app_framebuffer_mirror_invalidate();

  s_flush_y0 = fb->dirty_rect.origin.y;
  s_flush_y1 = fb->dirty_rect.origin.y + fb->dirty_rect.size.h - 1;
  s_update_complete_handler = handle_update_complete_cb;
  s_flush_start_ticks = pbl_uptime_ticks();

  prv_corners_apply(fb, true);

  struct pbl_display_buffer buf = {
    .data = (uint8_t *)framebuffer_get_line(fb, 0),
    .pitch = FRAMEBUFFER_BYTES_PER_ROW,
  };

  if (pbl_display_update(DISPLAY, &buf, s_flush_y0, s_flush_y1, prv_flush_done, nullptr) != 0) {
    prv_flush_finish((void *)1);
  }
}

bool compositor_display_update_in_progress(void) {
  return pbl_display_update_in_progress(DISPLAY);
}

static void prv_collect_display_stats(void) {
  pbl_irq_lock();
  uint32_t count = s_flush_count;
  uint64_t ticks = s_flush_ticks;
  s_flush_count = 0;
  s_flush_ticks = 0;
  pbl_irq_unlock();

  PBL_ANALYTICS_SET_UNSIGNED(display_update_count, count);
  PBL_ANALYTICS_SET_UNSIGNED(display_flush_time_ms, (uint32_t)((ticks * 1000) / PBL_TICK_HZ));
}
PBL_ANALYTICS_COLLECTOR_DEFINE(prv_collect_display_stats);
