/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <stdio.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/time.h>

#include <demos/lv_demos.h>
#include <pbl_lvgl.h>

#define SCENE_MS 1000
#define POLL_MS  50

int main(void) {
  int ret;

  ret = pbl_lvgl_init();
  if (ret != 0) {
    fprintf(stderr, "lvgl: init failed (%d)\n", ret);
    return ret;
  }

  printf("lvgl: render on %dx%d\n",
         (int)lv_display_get_horizontal_resolution(pbl_lvgl_get_display()),
         (int)lv_display_get_vertical_resolution(pbl_lvgl_get_display()));

  /*
   * The demos come with LVGL, each enabled by its CONFIG_LV_USE_DEMO_* option. This one draws
   * a scene per drawing primitive, laid out for a display of 480x272 or more.
   */
  for (int scene = 0; scene < LV_DEMO_RENDER_SCENE_NUM; scene++) {
    uint64_t end = pbl_uptime_ms() + SCENE_MS;

    lv_demo_render((lv_demo_render_scene_t)scene, LV_OPA_COVER);
    /* A scene that is drawn leaves LVGL nothing to do: it would wait forever. */
    while (pbl_uptime_ms() < end) {
      uint32_t next_ms = lv_timer_handler();

      pbl_sleep_ms(LV_MIN(next_ms, POLL_MS));
    }

    printf("lvgl: %s\n", lv_demo_render_get_scene_name((lv_demo_render_scene_t)scene));
  }

  printf("Sample done\n");

  return 0;
}
