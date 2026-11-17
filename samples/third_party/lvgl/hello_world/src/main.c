/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/time.h>

#include <pbl_lvgl.h>

#define SHOW_MS 5000

int main(void) {
  lv_obj_t *label;
  lv_obj_t *spinner;
  uint64_t end;
  int ret;

  ret = pbl_lvgl_init();
  if (ret != 0) {
    fprintf(stderr, "lvgl: init failed (%d)\n", ret);
    return ret;
  }

  label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, "Hello World!\n" CONFIG_BOARD);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, -40);

  /* An animation, so that there is something to render on every pass. */
  spinner = lv_spinner_create(lv_screen_active());
  lv_obj_set_size(spinner, 60, 60);
  lv_obj_align(spinner, LV_ALIGN_CENTER, 0, 30);

  printf("lvgl: %d.%d.%d, showing for %d s\n", lv_version_major(), lv_version_minor(),
         lv_version_patch(), SHOW_MS / 1000);

  /* LVGL does its work in lv_timer_handler(), which says when it is next due. */
  end = pbl_uptime_ms() + SHOW_MS;
  while (pbl_uptime_ms() < end) {
    pbl_sleep_ms(lv_timer_handler());
  }

  printf("Sample done\n");

  return 0;
}
