/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdio.h>

#include <pbl/kernel/delay.h>
#include <pbl/kernel/time.h>

#include <pbl_lvgl.h>

#define LISTEN_S 30

static lv_obj_t *s_label;
static int s_count;
static int s_clicks;

/* Runs from lv_timer_handler(), on the thread that calls it. */
static void prv_clicked(lv_event_t *e) {
  s_count += (int)(intptr_t)lv_event_get_user_data(e);
  s_clicks++;

  lv_label_set_text_fmt(s_label, "%d", s_count);
  printf("lvgl: count %d\n", s_count);
}

static void prv_add_button(lv_obj_t *parent, const char *text, int step) {
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_t *label = lv_label_create(button);

  lv_obj_set_size(button, 56, 44);
  lv_obj_add_event_cb(button, prv_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)step);
  lv_label_set_text(label, text);
  lv_obj_center(label);
}

int main(void) {
  lv_obj_t *row;
  uint64_t end;
  int ret;

  ret = pbl_lvgl_init();
  if (ret != 0) {
    fprintf(stderr, "lvgl: init failed (%d)\n", ret);
    return ret;
  }

  printf("lvgl: keypad: %s, pointer: %s\n", pbl_lvgl_get_keypad() != nullptr ? "yes" : "no",
         pbl_lvgl_get_pointer() != nullptr ? "yes" : "no");

  s_label = lv_label_create(lv_screen_active());
  lv_label_set_text(s_label, "0");
  lv_obj_align(s_label, LV_ALIGN_CENTER, 0, -45);

  row = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, 12, 0);
  /* Room for the outline a theme may draw around the focused button. */
  lv_obj_set_style_pad_all(row, 4, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, 20);

  /*
   * Widgets that take input join the default group as they are created: Up and Down move the
   * focus between them and Select clicks the focused one, as a touch does on any of them.
   */
  prv_add_button(row, LV_SYMBOL_MINUS, -1);
  prv_add_button(row, LV_SYMBOL_PLUS, 1);

  printf("lvgl: listening for %d s\n", LISTEN_S);

  end = pbl_uptime_ms() + LISTEN_S * 1000U;
  while (pbl_uptime_ms() < end) {
    pbl_sleep_ms(lv_timer_handler());
  }

  printf("lvgl: %d clicks\n", s_clicks);
  printf("Sample done\n");

  return 0;
}
