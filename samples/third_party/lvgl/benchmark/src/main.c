/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/kernel/delay.h>

#include <demos/lv_demos.h>
#include <pbl_lvgl.h>

static bool s_done;

/* Runs from lv_timer_handler(), after the last scene. */
static void prv_end(const lv_demo_benchmark_summary_t *summary) {
  for (const lv_demo_benchmark_scene_dsc_t *scene = summary->scenes; scene->create_cb != nullptr;
       scene++) {
    if (scene->measurement_cnt == 0U) {
      continue;
    }
    printf("lvgl: %s: %d fps, %d%% CPU, render %d ms, flush %d ms\n", scene->name,
           (int)(scene->fps_avg / scene->measurement_cnt),
           (int)(scene->cpu_avg_usage / scene->measurement_cnt),
           (int)(scene->render_avg_time / scene->measurement_cnt),
           (int)(scene->flush_avg_time / scene->measurement_cnt));
  }

  /* The totals are sums of the averages of the scenes. */
  if (summary->valid_scene_cnt > 0) {
    printf("lvgl: %d scenes: %d fps, %d%% CPU, render %d ms, flush %d ms\n",
           (int)summary->valid_scene_cnt, (int)(summary->total_avg_fps / summary->valid_scene_cnt),
           (int)(summary->total_avg_cpu / summary->valid_scene_cnt),
           (int)(summary->total_avg_render_time / summary->valid_scene_cnt),
           (int)(summary->total_avg_flush_time / summary->valid_scene_cnt));
  }

  s_done = true;
}

int main(void) {
  int ret;

  ret = pbl_lvgl_init();
  if (ret != 0) {
    fprintf(stderr, "lvgl: init failed (%d)\n", ret);
    return ret;
  }

  printf("lvgl: benchmark on %dx%d\n",
         (int)lv_display_get_horizontal_resolution(pbl_lvgl_get_display()),
         (int)lv_display_get_vertical_resolution(pbl_lvgl_get_display()));

  /* The demos come with LVGL, each enabled by its CONFIG_LV_USE_DEMO_* option. */
  lv_demo_benchmark_set_end_cb(prv_end);
  lv_demo_benchmark();

  while (!s_done) {
    pbl_sleep_ms(lv_timer_handler());
  }

  printf("Sample done\n");

  return 0;
}
