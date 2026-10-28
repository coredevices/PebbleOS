/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <string.h>

#include <pbl/drivers/display.h>
#include <pbl/drivers/rtc.h>
#include <pbl/kernel/irq.h>
#include <pbl/services/analytics/analytics.h>
#include <pbl/services/compositor/compositor.h>
#include <pbl/services/compositor/compositor_private.h>
#include <pbl/util/math.h>
#include <pbl/util/size.h>

#include <applib/graphics/framebuffer.h>
#include <applib/graphics/gcolor_definitions.h>
#include <applib/graphics/gtypes.h>
#include <kernel/events.h>

static void (*s_update_complete_handler)(bool success);

static RtcTicks s_flush_start_ticks;
static uint32_t s_flush_count;
static RtcTicks s_flush_ticks;

static uint16_t s_flush_y0;
static uint16_t s_flush_y1;

// Rounded corners, drawn into the framebuffer for the flush and undone after it. Each entry is
// the number of pixels masked from the left and right edges of a row from the top or bottom.
#ifdef CONFIG_BOARD_ASTERIX
static const uint8_t s_corner_shape[] = {3, 1, 1};
#elifdef CONFIG_BOARD_OBELIX
static const uint8_t s_corner_shape[] = {3, 3, 2, 1};
#endif

#if defined(CONFIG_BOARD_ASTERIX) || defined(CONFIG_BOARD_OBELIX)
#define CORNER_ROWS      ARRAY_LENGTH(s_corner_shape)
#define CORNER_MAX_WIDTH 3

#if PBL_BW
#define PIXEL_BYTE(x) ((x) / 8)
#else
#define PIXEL_BYTE(x) (x)
#endif

// Bytes holding the corner pixels of each masked row: [row][left, right][byte]
static uint8_t s_saved_corners[CORNER_ROWS * 2][2][CORNER_MAX_WIDTH];

static void prv_mask_pixel(uint8_t *line, uint16_t x) {
#if PBL_BW
  line[x / 8] &= ~(1U << (x % 8));
#else
  line[x] = GColorBlackARGB8;
#endif
}

static void prv_corner_row(uint16_t y, uint8_t *width, uint8_t *save_idx) {
  if (y < CORNER_ROWS) {
    *width = s_corner_shape[y];
    *save_idx = y;
  } else if (y >= DISP_ROWS - CORNER_ROWS) {
    *width = s_corner_shape[DISP_ROWS - y - 1];
    *save_idx = CORNER_ROWS + (DISP_ROWS - y - 1);
  } else {
    *width = 0;
  }
}

static void prv_corners_apply(FrameBuffer *fb, bool mask) {
  for (uint16_t y = s_flush_y0; y <= s_flush_y1; y++) {
    uint8_t width;
    uint8_t idx;

    prv_corner_row(y, &width, &idx);
    if (width == 0) {
      continue;
    }

    uint8_t *line = (uint8_t *)framebuffer_get_line(fb, y);
    uint16_t left = PIXEL_BYTE(0);
    uint16_t right = PIXEL_BYTE(DISP_COLS - width);
    uint16_t len = PIXEL_BYTE(width - 1) - left + 1;

    if (mask) {
      memcpy(s_saved_corners[idx][0], &line[left], len);
      memcpy(s_saved_corners[idx][1], &line[right], len);
      for (uint8_t x = 0; x < width; x++) {
        prv_mask_pixel(line, x);
        prv_mask_pixel(line, DISP_COLS - x - 1);
      }
    } else {
      memcpy(&line[left], s_saved_corners[idx][0], len);
      memcpy(&line[right], s_saved_corners[idx][1], len);
    }
  }
}
#else
static void prv_corners_apply(FrameBuffer *fb, bool mask) {
}
#endif

static void prv_flush_finish(void *data) {
  bool success = (data == nullptr);

  prv_corners_apply(compositor_get_framebuffer(), false);

  RtcTicks elapsed = rtc_get_ticks() - s_flush_start_ticks;
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
  s_flush_start_ticks = rtc_get_ticks();

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

void pbl_analytics_external_collect_display_stats(void) {
  pbl_irq_lock();
  uint32_t count = s_flush_count;
  RtcTicks ticks = s_flush_ticks;
  s_flush_count = 0;
  s_flush_ticks = 0;
  pbl_irq_unlock();

  PBL_ANALYTICS_SET_UNSIGNED(display_update_count, count);
  PBL_ANALYTICS_SET_UNSIGNED(display_flush_time_ms, (uint32_t)((ticks * 1000) / RTC_TICKS_HZ));
}
