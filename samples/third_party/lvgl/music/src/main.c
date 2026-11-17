/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/kernel/delay.h>

#include <demos/lv_demos.h>
#include <pbl_lvgl.h>

int main(void) {
  int ret;

  ret = pbl_lvgl_init();
  if (ret != 0) {
    fprintf(stderr, "lvgl: init failed (%d)\n", ret);
    return ret;
  }

  /* The demos come with LVGL, each enabled by its CONFIG_LV_USE_DEMO_* option. */
  lv_demo_music();

  printf("lvgl: music player on %dx%d\n",
         (int)lv_display_get_horizontal_resolution(pbl_lvgl_get_display()),
         (int)lv_display_get_vertical_resolution(pbl_lvgl_get_display()));

  while (true) {
    pbl_sleep_ms(lv_timer_handler());
  }
}
