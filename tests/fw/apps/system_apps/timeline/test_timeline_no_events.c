/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system_apps/timeline/timeline.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "services/normal/timeline/timeline_resources.h"

#include "test_timeline_app_includes.h"


// Setup and Teardown
////////////////////////////////////

typedef struct TimelineTestData {
  TimelineAppData app;
} TimelineTestData;

TimelineTestData s_data;

void test_timeline_no_events__initialize(void) {
  fake_app_state_init();
  load_system_resources_fixture();

  s_data = (TimelineTestData) {};
}

void test_timeline_no_events__cleanup(void) {
}

// Helpers
//////////////////////

void prv_init_peek_layer(TimelineAppData *data);
void prv_setup_no_events_peek(TimelineAppData *data);

static void prv_create_no_events_and_render(bool past) {
  Window *window = &s_data.app.timeline_window;
  window_init(window, "Timeline");
  const GColor color = past ? TIMELINE_PAST_COLOR : TIMELINE_FUTURE_COLOR;
  window_set_background_color(window, color);

  prv_init_peek_layer(&s_data.app);
  prv_setup_no_events_peek(&s_data.app);
  peek_layer_play(&s_data.app.peek_layer);
  fake_animation_complete(s_data.app.peek_layer.kino_layer.player.animation);
  fake_evented_timer_trigger(s_data.app.peek_layer.hidden_fields_timer);

  window_set_on_screen(window, true, true);
  window_render(window, fake_graphics_context_get_context());
}

// Tests
//////////////////////

void test_timeline_no_events__future(void) {
  prv_create_no_events_and_render(false /* past */);
  FAKE_GRAPHICS_CONTEXT_CHECK_DEST_BITMAP_FILE();
}

void test_timeline_no_events__past(void) {
  prv_create_no_events_and_render(true /* past */);
  FAKE_GRAPHICS_CONTEXT_CHECK_DEST_BITMAP_FILE();
}
