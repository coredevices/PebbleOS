/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/kernel/delay.h>

#include <demos/lv_demos.h>
#include <pbl_lvgl.h>

int main(void) {
  lv_mem_monitor_t mem;
  int ret;

  ret = pbl_lvgl_init();
  if (ret != 0) {
    fprintf(stderr, "lvgl: init failed (%d)\n", ret);
    return ret;
  }

  printf("lvgl: stress on %dx%d\n",
         (int)lv_display_get_horizontal_resolution(pbl_lvgl_get_display()),
         (int)lv_display_get_vertical_resolution(pbl_lvgl_get_display()));

  /* The demos come with LVGL, each enabled by its CONFIG_LV_USE_DEMO_* option. */
  lv_demo_stress();

  /*
   * One round creates and deletes every kind of object, with their animations. The demo is
   * between two rounds until its first step runs.
   */
  while (lv_demo_stress_finished()) {
    pbl_sleep_ms(lv_timer_handler());
  }
  while (!lv_demo_stress_finished()) {
    pbl_sleep_ms(lv_timer_handler());
  }

  lv_mem_monitor(&mem);
  printf("lvgl: one round, at most %u bytes of memory used\n", (unsigned int)mem.max_used);
  printf("Sample done\n");

  return 0;
}
