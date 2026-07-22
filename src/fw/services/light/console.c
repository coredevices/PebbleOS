/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <inttypes.h>

#include "console/prompt.h"
#include <pbl/drivers/ambient_light.h>
#include "kernel/util/sleep.h"
#include "pbl/services/light.h"

#if defined(CONFIG_ALS_SCREEN_COMPENSATION)
#include "applib/graphics/framebuffer.h"
#include "applib/ui/animation_private.h"
#include <pbl/drivers/task_watchdog.h>
#include <pbl/drivers/watchdog.h>
#include "kernel/event_loop.h"
#include "pbl/services/compositor/compositor.h"
#include "pbl/services/compositor/compositor_display.h"
#include "services/light/als_screen_compensation.h"

#include <string.h>
#endif

// Let the sensor settle after a backlight-off transition before reading.
#define ALS_LUX_SETTLE_MS (250)

void command_light_test(void) {
  char buffer[64];
#if defined(CONFIG_ALS_SCREEN_COMPENSATION)
  // Print raw, region luminance, and the corrected value to sanity-check the
  // under-display compensation.
  const uint32_t raw = ambient_light_get_light_level();
  const uint16_t lum_q8 = als_compensation_sample_luminance();
  const uint32_t corr = als_compensation_apply(raw, lum_q8, CONFIG_ALS_BLACK_SCALE_Q8);
  prompt_send_response_fmt(buffer, sizeof(buffer),
                           "als raw: %" PRIu32 " lum_q8: %" PRIu16 " corr: %" PRIu32,
                           raw, lum_q8, corr);
#else
  prompt_send_response_fmt(buffer, sizeof(buffer), "als: %" PRIu32,
                           ambient_light_get_light_level());
#endif
  light_enable_interaction();
  prompt_send_response_fmt(buffer, sizeof(buffer), "brightness: %" PRIu8 "%%",
                           light_get_current_brightness_percent());
}

// Print raw, compensated, and lux values in one shot. Forces the backlight off
// for the read: the light service suspends ALS sampling while the LED is on,
// so a reading taken then would be a stale cached value.
void command_als_lux(void) {
  char buffer[96];
  light_allow(false);
  psleep(ALS_LUX_SETTLE_MS);
  const uint32_t raw = ambient_light_get_light_level();
#if defined(CONFIG_ALS_SCREEN_COMPENSATION)
  const uint32_t corr =
      als_compensation_apply(raw, als_compensation_sample_luminance(), CONFIG_ALS_BLACK_SCALE_Q8);
#else
  const uint32_t corr = raw;
#endif
  light_allow(true);
  if (ambient_light_lux_available()) {
    prompt_send_response_fmt(buffer, sizeof(buffer),
                             "als raw: %" PRIu32 " corr: %" PRIu32 " lux: %" PRIu32, raw, corr,
                             ambient_light_level_to_lux(corr));
  } else {
    prompt_send_response_fmt(buffer, sizeof(buffer),
                             "als raw: %" PRIu32 " corr: %" PRIu32 " lux: n/a", raw, corr);
  }
}

#if defined(CONFIG_ALS_SCREEN_COMPENSATION)

// Settle for a solid-fill read (panel + sensor integration).
#define ALS_MEASURE_SETTLE_MS (250)

// Opaque GColor8 bytes (a=3).
#define ALS_PX_BLACK (0xC0)
#define ALS_PX_WHITE (0xFF)

// Set by prv_als_flush_cb on KernelMain once the panel update is kicked.
static volatile bool s_als_flush_done;

// Runs on KernelMain (the compositor's task): push the staged system framebuffer
// to the panel, the same way the PULSE framebuffer domain does.
static void prv_als_flush_cb(void *unused) {
  FrameBuffer *fb = compositor_get_framebuffer();
  framebuffer_dirty_all(fb);
  compositor_display_update(NULL);
  s_als_flush_done = true;
}

// Fill the whole screen with one GColor8 byte and block until the pixels are up.
static void prv_als_fill_solid(uint8_t px) {
  FrameBuffer *fb = compositor_get_framebuffer();
  memset(fb->buffer, px, FRAMEBUFFER_SIZE_BYTES);
  s_als_flush_done = false;
  launcher_task_add_callback(prv_als_flush_cb, NULL);
  while (!s_als_flush_done) {
    psleep(2);
  }
  while (compositor_display_update_in_progress()) {
    psleep(2);
  }
}

// Average a few raw (uncompensated) ALS reads to damp noise.
static uint32_t prv_als_read_raw_avg(uint8_t n) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < n; i++) {
    sum += ambient_light_get_light_level();
    watchdog_feed();
    task_watchdog_bit_set_all();
    psleep(30);
  }
  return (n > 0) ? (sum / n) : 0;
}

// Measure raw ALS at the four luminance levels 8-bit color can display (black /
// dark-gray / light-gray / white -> lum_q8 0/85/170/256) at the current ambient,
// printing raw and gain-vs-white at each. Run at several ambient levels to map
// the transmittance curve.
void command_als_curve(void) {
  char buf[80];
  static const struct {
    const char *name;
    uint8_t px;
    uint16_t lum_q8;
  } levels[] = {
      {"white", ALS_PX_WHITE, 256},
      {"lgray", 0xEA, 170},
      {"dgray", 0xD5, 85},
      {"black", ALS_PX_BLACK, 0},
  };

  animation_private_pause();
  compositor_freeze();
  while (compositor_display_update_in_progress()) {
    psleep(2);
  }

  uint32_t raw_white = 1;
  for (uint8_t i = 0; i < (uint8_t)(sizeof(levels) / sizeof(levels[0])); i++) {
    prv_als_fill_solid(levels[i].px);
    psleep(ALS_MEASURE_SETTLE_MS);
    const uint32_t raw = prv_als_read_raw_avg(4);
    if (i == 0) {
      raw_white = (raw > 0) ? raw : 1;
    }
    const uint32_t gain_x100 = (raw > 0) ? (raw_white * 100u / raw) : 0;
    prompt_send_response_fmt(buf, sizeof(buf),
                             "als curve: %s lum_q8=%" PRIu16 " raw=%" PRIu32
                             " gain=%" PRIu32 ".%02" PRIu32 "x",
                             levels[i].name, levels[i].lum_q8, raw,
                             gain_x100 / 100u, gain_x100 % 100u);
  }

  compositor_unfreeze();
  animation_private_resume();
  prompt_send_response("als: press a button to repaint");
}

#endif  // CONFIG_ALS_SCREEN_COMPENSATION
