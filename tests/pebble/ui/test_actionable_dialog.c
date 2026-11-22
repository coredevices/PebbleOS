/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdlib.h>

#include <applib/graphics/framebuffer.h>
#include <applib/graphics/graphics.h>
#include <applib/ui/dialogs/actionable_dialog.h>
#include <applib/ui/window_private.h>
#include <clar.h>
#include <resource/resource.h>
#include <resource/resource_ids.auto.h>
#include <shell/prefs.h>
#include <shell/system_theme.h>

static GContext s_ctx;
static FrameBuffer *fb = nullptr;

// Fakes
/////////////////////

#include <fake_content_indicator.h>
#include <fake_pfs.h>
#include <fake_spi_flash.h>
#include <fixtures/load_test_resources.h>

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

// Stubs
/////////////////////

#include <stubs_buffer.h>

AnimationProgress animation_timing_scaled(AnimationProgress time_normalized,
                                          AnimationProgress interval_start,
                                          AnimationProgress interval_end) {
  return interval_end;
}

AnimationProgress animation_timing_curve(AnimationProgress time_normalized, AnimationCurve curve) {
  return time_normalized;
}

KinoReel *kino_reel_scale_segmented_create(KinoReel *from_reel, bool take_ownership,
                                           GRect screen_frame) {
  return nullptr;
}

void kino_reel_scale_segmented_set_deflate_effect(KinoReel *reel, int16_t expand) {
}

bool kino_reel_scale_segmented_set_delay_by_distance(KinoReel *reel, GPoint target) {
  return false;
}

void kino_reel_transform_set_from_frame(KinoReel *reel, GRect from) {
}

void kino_reel_transform_set_transform_duration(KinoReel *reel, uint32_t duration) {
}

uint16_t time_ms(time_t *tloc, uint16_t *out_ms) {
  return 0;
}

// Setup and Teardown
////////////////////////////////////

void test_actionable_dialog__initialize(void) {
  display_orientation_set_left(false);
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize){PBL_DISPLAY_WIDTH, PBL_DISPLAY_HEIGHT});
  // Must use System init mode to enable orphan avoidance algorithm
  const GContextInitializationMode context_init_mode = GContextInitializationMode_System;
  graphics_context_init(&s_ctx, fb, context_init_mode);
  framebuffer_clear(fb);

  fake_spi_flash_init(0 /* offset */, 0x1000000 /* length */);
  fake_pfs_mount();
  pbl_fs_format("/");
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);
  resource_init();
}

void test_actionable_dialog__cleanup(void) {
  free(fb);
}

// Helpers
////////////////////////////////////

#include "../graphics/util.h"

static ActionableDialog *prv_create_dialog(void) {
  ActionableDialog *actionable_dialog = actionable_dialog_create("Alignment");
  actionable_dialog_set_action_bar_type(actionable_dialog, DialogActionBarConfirm, nullptr);

  Dialog *dialog = actionable_dialog_get_dialog(actionable_dialog);
  dialog_set_text(dialog, "Clear history?");
  dialog_set_icon(dialog, RESOURCE_ID_GENERIC_QUESTION_LARGE);

  window_set_on_screen(&dialog->window, true, true);
  window_render(&dialog->window, &s_ctx);
  return actionable_dialog;
}

static void prv_assert_content_centered(ActionableDialog *actionable_dialog) {
  const GRect icon_frame = actionable_dialog->dialog.icon_layer.layer.frame;
  const GRect text_frame = actionable_dialog->dialog.text_layer.layer.frame;

  cl_assert_equal_i(icon_frame.origin.x + icon_frame.size.w / 2, PBL_DISPLAY_WIDTH / 2);
  cl_assert_equal_i(text_frame.origin.x + text_frame.size.w / 2, PBL_DISPLAY_WIDTH / 2);
  cl_assert_equal_i(actionable_dialog->dialog.text_layer.text_alignment, GTextAlignmentCenter);
  cl_assert(text_frame.origin.x >= ACTION_BAR_WIDTH);
  cl_assert(grect_get_max_x(&text_frame) <= PBL_DISPLAY_WIDTH - ACTION_BAR_WIDTH);
}

// Tests
////////////////////////////////////

void test_actionable_dialog__content_is_centered(void) {
  prv_assert_content_centered(prv_create_dialog());
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}

void test_actionable_dialog__left_hand_content_is_centered(void) {
  display_orientation_set_left(true);
  prv_assert_content_centered(prv_create_dialog());
  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
