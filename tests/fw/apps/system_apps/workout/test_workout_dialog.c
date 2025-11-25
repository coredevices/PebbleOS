/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/system_apps/workout/workout_dialog.h"

#include "resource/resource_ids.auto.h"

#include "test_workout_app_includes.h"

// Fakes
/////////////////////

uint16_t time_ms(time_t *tloc, uint16_t *out_ms) {
  return 0;
}

// Setup and Teardown
////////////////////////////////////

static GContext s_ctx;
static FrameBuffer s_fb;

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

void test_workout_dialog__initialize(void) {
  // Setup graphics context
  framebuffer_init(&s_fb, &(GSize) {DISP_COLS, DISP_ROWS});
  framebuffer_clear(&s_fb);
  graphics_context_init(&s_ctx, &s_fb, GContextInitializationMode_App);
  s_app_state_get_graphics_context = &s_ctx;

  // Setup resources
  fake_spi_flash_init(0 /* offset */, 0x1000000 /* length */);
  pfs_init(false /* run filesystem check */);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);
  resource_init();

  // Setup content indicator
  ContentIndicatorsBuffer *buffer = content_indicator_get_current_buffer();
  content_indicator_init_buffer(buffer);
}

void test_workout_dialog__cleanup(void) {
}

// Tests
//////////////////////

void test_workout_dialog__render_end_workout(void) {
  WorkoutDialog *workout_dialog = workout_dialog_create("Workout End");
  Dialog *dialog = workout_dialog_get_dialog(workout_dialog);

  dialog_show_status_bar_layer(dialog, true);
  dialog_set_fullscreen(dialog, true);
  dialog_set_text(dialog, "End Workout?");
  dialog_set_background_color(dialog, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  dialog_set_text_color(dialog, GColorBlack);
  dialog_set_icon(dialog, RESOURCE_ID_WORKOUT_APP_END);
  dialog_set_icon_animate_direction(dialog, DialogIconAnimateNone);

  window_set_on_screen(&dialog->window, true, true);
  window_render(&dialog->window, &s_ctx);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_dialog__render_detected_workout(void) {
  WorkoutDialog *workout_dialog = workout_dialog_create("Workout Detected");
  Dialog *dialog = workout_dialog_get_dialog(workout_dialog);

  dialog_show_status_bar_layer(dialog, true);
  dialog_set_fullscreen(dialog, true);
  dialog_set_background_color(dialog, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  dialog_set_text_color(dialog, GColorBlack);
  dialog_set_icon(dialog, RESOURCE_ID_WORKOUT_APP_DETECTED);
  dialog_set_icon_animate_direction(dialog, DialogIconAnimateNone);

  workout_dialog_set_text(workout_dialog, "Run\nDetected");
  workout_dialog_set_subtext(workout_dialog, "03:42");

  window_set_on_screen(&dialog->window, true, true);
  window_render(&dialog->window, &s_ctx);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_workout_dialog__render_workout_ended(void) {
  WorkoutDialog *workout_dialog = workout_dialog_create("Workout Ended");
  Dialog *dialog = workout_dialog_get_dialog(workout_dialog);

  dialog_show_status_bar_layer(dialog, true);
  dialog_set_fullscreen(dialog, true);
  dialog_set_background_color(dialog, PBL_IF_COLOR_ELSE(GColorYellow, GColorWhite));
  dialog_set_text_color(dialog, GColorBlack);
  dialog_set_icon(dialog, RESOURCE_ID_WORKOUT_APP_DETECTED);
  dialog_set_icon_animate_direction(dialog, DialogIconAnimateNone);

  workout_dialog_set_text(workout_dialog, "Workout\nEnded");
  workout_dialog_set_action_bar_hidden(workout_dialog, true);

  window_set_on_screen(&dialog->window, true, true);
  window_render(&dialog->window, &s_ctx);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
