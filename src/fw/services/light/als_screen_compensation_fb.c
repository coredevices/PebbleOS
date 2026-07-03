/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "services/light/als_screen_compensation.h"

#if defined(CONFIG_ALS_SCREEN_COMPENSATION)

#include "applib/graphics/gtypes.h"
#include "board/board.h"
#include "pbl/services/compositor/compositor.h"
#ifdef CONFIG_ORIENTATION_MANAGER
#include "shell/prefs.h"
#endif

uint16_t als_compensation_sample_luminance(void) {
  int16_t rx = CONFIG_ALS_SENSOR_X;
  int16_t ry = CONFIG_ALS_SENSOR_Y;
  const int16_t rw = CONFIG_ALS_SENSOR_W;
  const int16_t rh = CONFIG_ALS_SENSOR_H;
#ifdef CONFIG_ORIENTATION_MANAGER
  // The region is measured in the default orientation; wearing the watch on the
  // other wrist flips the display 180deg, so the framebuffer area in front of
  // the sensor mirrors. This is a relative flip, independent of the board's
  // default rotation.
  if (display_orientation_is_left()) {
    rx = DISP_COLS - rx - rw;
    ry = DISP_ROWS - ry - rh;
  }
#endif
  // The system framebuffer is owned by KernelMain, the same task the light
  // service runs on, so reading it here cannot race a composite.
  GBitmap fb = compositor_get_framebuffer_as_bitmap();
  return als_compensation_region_luminance(&fb, rx, ry, rw, rh);
}

uint32_t als_compensation_correct(uint32_t raw_level) {
  return als_compensation_apply(raw_level, als_compensation_sample_luminance(),
                                CONFIG_ALS_BLACK_SCALE_Q8);
}

#endif  // CONFIG_ALS_SCREEN_COMPENSATION
