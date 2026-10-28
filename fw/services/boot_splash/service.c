/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/delay.h>
#include <pbl/kernel/thread.h>
#include <pbl/services/boot_splash.h>

#if defined(CONFIG_PBLBOOT) || defined(CONFIG_QEMU) || defined(CONFIG_SOC_POSIX)

#include <string.h>

#include <pbl/drivers/display.h>
#include <pbl/util/math.h>

#include <board/splash.h>
#include <kernel/pbl_malloc.h>

enum splash_color {
  SPLASH_COLOR_WHITE,
  SPLASH_COLOR_BLACK,
  SPLASH_COLOR_LGRAY,
};

// ARGB2222 values of the splash colors
static const uint8_t s_argb2222[] = {
  [SPLASH_COLOR_WHITE] = 0xFF,
  [SPLASH_COLOR_BLACK] = 0xC0,
  [SPLASH_COLOR_LGRAY] = 0xEA,
};

// Progress bar configuration
#define PROGRESS_BAR_WIDTH       80
#define PROGRESS_BAR_HEIGHT      4
#define PROGRESS_INDICATOR_WIDTH 20
#define PROGRESS_FRAME_DELAY_MS  100
#define PROGRESS_TOTAL_FRAMES    20

#define BOOT_SPLASH_TASK_STACK_SIZE 2048
#define BOOT_SPLASH_TASK_PRIORITY   (PBL_PRIO_MAX - 1)

// Boot splash state
static struct pbl_thread s_boot_splash_thread;
PBL_THREAD_STACK_DEFINE(s_boot_splash_stack, BOOT_SPLASH_TASK_STACK_SIZE);
static struct pbl_thread *s_boot_splash_task;
static volatile bool s_boot_splash_running;
static struct pbl_display_buffer s_boot_splash_fb;

static uint32_t prv_pitch(void) {
  if (DISPLAY->format == PBL_DISPLAY_FORMAT_MONO) {
    return ROUND_TO_MOD_CEIL(DISPLAY->width, 32U) / 8U;
  }

  return DISPLAY->width;
}

static size_t prv_fb_bytes(void) {
  return prv_pitch() * DISPLAY->height;
}

static void prv_set_pixel(const struct pbl_display_buffer *fb, int16_t x, int16_t y,
                          enum splash_color color) {
  if (x < 0 || x >= DISPLAY->width || y < 0 || y >= DISPLAY->height) {
    return;
  }

  uint8_t *row = &fb->data[(uint32_t)y * fb->pitch];

  if (DISPLAY->format == PBL_DISPLAY_FORMAT_MONO) {
    // Light gray shows as white, the indicator as black
    if (color == SPLASH_COLOR_BLACK) {
      row[x / 8] &= ~(1U << (x % 8));
    } else {
      row[x / 8] |= 1U << (x % 8);
    }
  } else {
    row[x] = s_argb2222[color];
  }
}

static void prv_clear(const struct pbl_display_buffer *fb) {
  memset(fb->data,
         (DISPLAY->format == PBL_DISPLAY_FORMAT_MONO) ? 0xFF : s_argb2222[SPLASH_COLOR_WHITE],
         prv_fb_bytes());
}

// Draw a filled rectangle
static void prv_draw_filled_rect(const struct pbl_display_buffer *fb, int16_t x0, int16_t y0,
                                 int16_t width, int16_t height, enum splash_color color) {
  for (int16_t y = y0; y < y0 + height; y++) {
    for (int16_t x = x0; x < x0 + width; x++) {
      prv_set_pixel(fb, x, y, color);
    }
  }
}

// Draw the logo, centered
static void prv_draw_logo(const struct pbl_display_buffer *fb) {
  int16_t logo_x0 = (DISPLAY->width - splash_width) / 2;
  int16_t logo_y0 = (DISPLAY->height - splash_height) / 2;

  for (uint16_t y = 0U; y < splash_height; y++) {
    for (uint16_t x = 0U; x < splash_width; x++) {
      if (splash_bits[y * (splash_width / 8) + x / 8] & (0x1U << (x & 7))) {
        prv_set_pixel(fb, x + logo_x0, y + logo_y0, SPLASH_COLOR_BLACK);
      }
    }
  }
}

// Draw progress bar with animated indicator
static void prv_draw_progress_bar(const struct pbl_display_buffer *fb, int16_t center_x,
                                  int16_t center_y, uint16_t frame) {
  int16_t bar_x0 = center_x - PROGRESS_BAR_WIDTH / 2;
  int16_t bar_y0 = center_y - PROGRESS_BAR_HEIGHT / 2;

  // Draw track (background)
  prv_draw_filled_rect(fb, bar_x0, bar_y0, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT,
                       SPLASH_COLOR_LGRAY);

  // Calculate indicator position (ping-pong animation)
  int32_t max_travel = PROGRESS_BAR_WIDTH - PROGRESS_INDICATOR_WIDTH;
  int32_t half_frames = PROGRESS_TOTAL_FRAMES / 2;
  int32_t cycle_frame = frame % PROGRESS_TOTAL_FRAMES;
  int32_t indicator_offset;

  if (cycle_frame < half_frames) {
    // Moving right
    indicator_offset = (cycle_frame * max_travel) / half_frames;
  } else {
    // Moving left
    indicator_offset = max_travel - ((cycle_frame - half_frames) * max_travel) / half_frames;
  }

  // Draw indicator
  prv_draw_filled_rect(fb, bar_x0 + indicator_offset, bar_y0, PROGRESS_INDICATOR_WIDTH,
                       PROGRESS_BAR_HEIGHT, SPLASH_COLOR_BLACK);
}

static void prv_show(const struct pbl_display_buffer *fb) {
  (void)pbl_display_update_sync(DISPLAY, fb, 0U, DISPLAY->height - 1U);
}

// Boot splash task - runs until stopped
static void prv_boot_splash_task(void *param) {
  const struct pbl_display_buffer *fb = &s_boot_splash_fb;
  uint16_t frame = 0;

  // Progress bar below the centered logo
  int16_t progress_cx = DISPLAY->width / 2;
  int16_t progress_cy = (DISPLAY->height - splash_height) / 2 + splash_height + 20;

  // Animation loop - runs until s_boot_splash_running is cleared
  while (s_boot_splash_running) {
    prv_clear(fb);
    prv_draw_logo(fb);
    prv_draw_progress_bar(fb, progress_cx, progress_cy, frame);
    prv_show(fb);

    frame++;

    // Wait for next frame using short delays to allow quick stop
    for (uint8_t i = 0; i < (PROGRESS_FRAME_DELAY_MS / 10) && s_boot_splash_running; i++) {
      pbl_thread_sleep(PBL_MSEC(10));
    }
  }

  // Cleanup
  kernel_free(s_boot_splash_fb.data);
  s_boot_splash_fb.data = nullptr;
  s_boot_splash_task = nullptr;
  pbl_thread_abort(nullptr);
}

void boot_splash_start(void) {
  if (pbl_device_init(&DISPLAY->dev) != 0) {
    return;
  }

  // Allocate framebuffer for boot splash
  s_boot_splash_fb = (struct pbl_display_buffer){
    .data = kernel_malloc(prv_fb_bytes()),
    .pitch = prv_pitch(),
  };
  if (s_boot_splash_fb.data == nullptr) {
    return;
  }

  // Start the boot splash task
  s_boot_splash_running = true;
  struct pbl_thread_attr attr = {
    .name = "BootSplash",
    .entry = prv_boot_splash_task,
    .prio = BOOT_SPLASH_TASK_PRIORITY,
    .privileged = true,
    .stack = s_boot_splash_stack,
    .stack_size = sizeof(s_boot_splash_stack),
  };
  if (pbl_thread_create(&s_boot_splash_thread, &attr) == 0) {
    s_boot_splash_task = &s_boot_splash_thread;
  }
}

void boot_splash_stop(void) {
  if (s_boot_splash_running && s_boot_splash_task != nullptr) {
    s_boot_splash_running = false;
    // Wait for the splash task to finish (max ~10ms delay due to short sleep intervals)
    while (s_boot_splash_task != nullptr) {
      pbl_sleep_ms(10);
    }

    // Draw final frame with logo only (no progress bar)
    struct pbl_display_buffer fb = {
      .data = kernel_malloc(prv_fb_bytes()),
      .pitch = prv_pitch(),
    };
    if (fb.data != nullptr) {
      prv_clear(&fb);
      prv_draw_logo(&fb);
      prv_show(&fb);
      kernel_free(fb.data);
    }
  }
}

#else // Everything else

void boot_splash_start(void) {
  // No-op on platforms where the bootloader handles the splash
}

void boot_splash_stop(void) {
  // No-op on platforms where the bootloader handles the splash
}

#endif
