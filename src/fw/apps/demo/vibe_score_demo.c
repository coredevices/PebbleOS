/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/app.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/number_window.h"
#include "process_management/pebble_process_md.h"
#include "process_state/app_state/app_state.h"
#include "resource/resource_ids.auto.h"
#include "services/normal/vibes/vibe_score.h"
#include "util/size.h"

#include <stdint.h>

static uint32_t s_vibe_score_resources[] = {
  RESOURCE_ID_VIBE_SCORE_NUDGE_NUDGE,
};
static VibeScore *s_vibe_scores[ARRAY_LENGTH(s_vibe_score_resources)];

static void prv_do_vibe(NumberWindow *nw, void *ctx) {
  int selected_idx = number_window_get_value(nw);
  if (s_vibe_scores[selected_idx]) {
    vibe_score_do_vibe(s_vibe_scores[selected_idx]);
  }
}

static void prv_load_scores(void) {
  int num_scores = ARRAY_LENGTH(s_vibe_scores);
  for (int i = 0; i < num_scores; i++) {
    s_vibe_scores[i] = vibe_score_create_with_resource(s_vibe_score_resources[i]);
  }
}

static void prv_unload_scores(void) {
  int num_scores = ARRAY_LENGTH(s_vibe_scores);
  for (int i = 0; i < num_scores; i++) {
    vibe_score_destroy(s_vibe_scores[i]);
  }
}

static void handle_init(void) {
  prv_load_scores();

  NumberWindow *vibe_num_window = number_window_create("Vibe Patterns",
      (NumberWindowCallbacks) { .selected = prv_do_vibe },
      NULL);
  app_state_set_user_data(vibe_num_window);

  number_window_set_value(vibe_num_window, 0);
  number_window_set_max(vibe_num_window, ARRAY_LENGTH(s_vibe_scores) - 1);
  number_window_set_min(vibe_num_window, 0);
  number_window_set_step_size(vibe_num_window, 1);

  app_window_stack_push(number_window_get_window(vibe_num_window), true);
}

static void handle_deinit(void) {
  prv_unload_scores();
  NumberWindow *data = app_state_get_user_data();
  number_window_destroy(data);
}

static void s_main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}

const PebbleProcessMd* vibe_score_demo_get_info() {
  static const PebbleProcessMdSystem s_vibe_score_info = {
    .common.main_func = s_main,
    .name = "Vibe Patterns"
  };
  return (const PebbleProcessMd*) &s_vibe_score_info;
}
