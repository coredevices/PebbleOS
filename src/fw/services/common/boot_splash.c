/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "boot_splash.h"

#if CAPABILITY_HAS_PBLBOOT

#include "board/display.h"
#include "board/splash.h"
#include "drivers/display/display.h"
#include "kernel/pbl_malloc.h"
#include "kernel/util/sleep.h"

#include "FreeRTOS.h"
#include "task.h"

#include <string.h>

// Progress bar configuration
#define PROGRESS_BAR_WIDTH      80
#define PROGRESS_BAR_HEIGHT     4
#define PROGRESS_INDICATOR_WIDTH 20
#define PROGRESS_FRAME_DELAY_MS 100
#define PROGRESS_TOTAL_FRAMES   20
#define BOOT_SPLASH_TASK_STACK_SIZE 512
#define BOOT_SPLASH_TASK_PRIORITY   (configMAX_PRIORITIES - 2)

// Boot splash state
static TaskHandle_t s_boot_splash_task;
static volatile bool s_boot_splash_running;
static uint8_t *s_boot_splash_fb;

// Draw a filled rectangle
static void prv_draw_filled_rect(uint8_t *fb, int16_t x0, int16_t y0,
                                 int16_t width, int16_t height, uint8_t color) {
  for (int16_t y = y0; y < y0 + height; y++) {
    for (int16_t x = x0; x < x0 + width; x++) {
      if (x >= 0 && x < PBL_DISPLAY_WIDTH && y >= 0 && y < PBL_DISPLAY_HEIGHT) {
        fb[(uint32_t)y * PBL_DISPLAY_WIDTH + x] = color;
      }
    }
  }
}

// Draw progress bar with animated indicator
static void prv_draw_progress_bar(uint8_t *fb, int16_t center_x, int16_t center_y,
                                  uint16_t frame) {
  const uint8_t COLOR_TRACK = 0xB6;      // Light gray in RGB332
  const uint8_t COLOR_INDICATOR = 0x00;  // Black

  int16_t bar_x0 = center_x - PROGRESS_BAR_WIDTH / 2;
  int16_t bar_y0 = center_y - PROGRESS_BAR_HEIGHT / 2;

  // Draw track (background)
  prv_draw_filled_rect(fb, bar_x0, bar_y0, PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT, COLOR_TRACK);

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
  prv_draw_filled_rect(fb, bar_x0 + indicator_offset, bar_y0,
                       PROGRESS_INDICATOR_WIDTH, PROGRESS_BAR_HEIGHT, COLOR_INDICATOR);
}

// Boot splash task - runs until stopped
static void prv_boot_splash_task(void *param) {
  uint8_t *fb = s_boot_splash_fb;
  uint16_t frame = 0;

  // Get splash logo dimensions
  const uint16_t logo_width = splash_width;
  const uint16_t logo_height = splash_height;
  const uint8_t *logo_data = splash_bits;

  // Calculate logo position (centered)
  uint16_t logo_x0 = (PBL_DISPLAY_WIDTH - logo_width) / 2;
  uint16_t logo_y0 = (PBL_DISPLAY_HEIGHT - logo_height) / 2;

  // Calculate progress bar center position (below centered logo)
  int16_t progress_cx = PBL_DISPLAY_WIDTH / 2;
  int16_t progress_cy = logo_y0 + logo_height + 20;

  // Animation loop - runs until s_boot_splash_running is cleared
  while (s_boot_splash_running) {
    // Clear framebuffer to white
    memset(fb, 0xFF, DISPLAY_FRAMEBUFFER_BYTES);

    // Draw logo
    for (uint16_t y = 0U; y < logo_height; y++) {
      for (uint16_t x = 0U; x < logo_width; x++) {
        if (logo_data[y * (logo_width / 8) + x / 8] & (0x1U << (x & 7))) {
          fb[(y + logo_y0) * PBL_DISPLAY_WIDTH + (x + logo_x0)] = 0x00;
        }
      }
    }

    // Draw progress bar
    prv_draw_progress_bar(fb, progress_cx, progress_cy, frame);

    // Send frame to display
    display_update_boot_frame(fb);

    frame++;

    // Wait for next frame using short delays to allow quick stop
    for (uint8_t i = 0; i < (PROGRESS_FRAME_DELAY_MS / 10) && s_boot_splash_running; i++) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }

  // Cleanup
  kernel_free(fb);
  s_boot_splash_fb = NULL;
  s_boot_splash_task = NULL;
  vTaskDelete(NULL);
}

void boot_splash_start(void) {
  // Initialize the display
  display_init();

  // Allocate framebuffer for boot splash
  s_boot_splash_fb = kernel_malloc(DISPLAY_FRAMEBUFFER_BYTES);
  if (!s_boot_splash_fb) {
    return;
  }

  // Start the boot splash task
  s_boot_splash_running = true;
  xTaskCreate(prv_boot_splash_task,
              "BootSplash",
              BOOT_SPLASH_TASK_STACK_SIZE,
              NULL,
              BOOT_SPLASH_TASK_PRIORITY,
              &s_boot_splash_task);
}

void boot_splash_stop(void) {
  if (s_boot_splash_running && s_boot_splash_task != NULL) {
    s_boot_splash_running = false;
    // Wait for the splash task to finish (max ~10ms delay due to short sleep intervals)
    while (s_boot_splash_task != NULL) {
      psleep(10);
    }

    // Draw final frame with logo only (no progress bar)
    uint8_t *fb = kernel_malloc(DISPLAY_FRAMEBUFFER_BYTES);
    if (fb) {
      memset(fb, 0xFF, DISPLAY_FRAMEBUFFER_BYTES);

      // Get splash logo dimensions
      const uint16_t logo_width = splash_width;
      const uint16_t logo_height = splash_height;
      const uint8_t *logo_data = splash_bits;

      // Calculate logo position (centered, same as animation)
      uint16_t logo_x0 = (PBL_DISPLAY_WIDTH - logo_width) / 2;
      uint16_t logo_y0 = (PBL_DISPLAY_HEIGHT - logo_height) / 2;

      // Draw logo
      for (uint16_t y = 0U; y < logo_height; y++) {
        for (uint16_t x = 0U; x < logo_width; x++) {
          if (logo_data[y * (logo_width / 8) + x / 8] & (0x1U << (x & 7))) {
            fb[(y + logo_y0) * PBL_DISPLAY_WIDTH + (x + logo_x0)] = 0x00;
          }
        }
      }

      display_update_boot_frame(fb);
      kernel_free(fb);
    }
  }
}

#elif CAPABILITY_HAS_FPGA_DISPLAY
#include "drivers/display/ice40lp/ice40lp_internal.h"
#include "drivers/display/ice40lp/snowy_boot.h"

// On platforms with FPGA display, we rely on the bootloader to show the splash screen.
void boot_splash_start(void) {
  display_start();
  display_spi_configure_default();
  boot_display_show_boot_splash();
}

void boot_splash_stop(void) {
  // No-op; the display driver handles stopping the splash as needed
}

#else // Everything else

void boot_splash_start(void) {
  // No-op on platforms where the bootloader handles the splash
}

void boot_splash_stop(void) {
  // No-op on platforms where the bootloader handles the splash
}

#endif
