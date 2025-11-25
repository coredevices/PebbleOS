/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "applib/graphics/framebuffer.h"
#include "applib/graphics/graphics.h"
#include "applib/ui/app_window_stack.h"
#include "applib/ui/content_indicator.h"
#include "applib/ui/content_indicator_private.h"
#include "applib/ui/dialogs/expandable_dialog.h"
#include "applib/ui/text_layer.h"
#include "applib/ui/window_private.h"
#include "resource/resource.h"
#include "resource/resource_ids.auto.h"
#include "services/normal/timeline/timeline_resources.h"
#include "shell/system_theme.h"
#include "util/graphics.h"
#include "util/hash.h"
#include "util/math.h"
#include "util/size.h"

#include "clar.h"

#include <stdio.h>

static GContext s_ctx;

// Fakes
/////////////////////

#include "fake_content_indicator.h"
#include "fake_spi_flash.h"
#include "fixtures/load_test_resources.h"

GContext *graphics_context_get_current_context(void) {
  return &s_ctx;
}

// Stubs
/////////////////////

#include "stubs_analytics.h"
#include "stubs_app_install_manager.h"
#include "stubs_app_state.h"
#include "stubs_app_timer.h"
#include "stubs_bootbits.h"
#include "stubs_buffer.h"
#include "stubs_click.h"
#include "stubs_heap.h"
#include "stubs_layer.h"
#include "stubs_logging.h"
#include "stubs_memory_layout.h"
#include "stubs_mutex.h"
#include "stubs_passert.h"
#include "stubs_pbl_malloc.h"
#include "stubs_pebble_tasks.h"
#include "stubs_print.h"
#include "stubs_process_manager.h"
#include "stubs_prompt.h"
#include "stubs_serial.h"
#include "stubs_shell_prefs.h"
#include "stubs_sleep.h"
#include "stubs_status_bar_layer.h"
#include "stubs_syscall_internal.h"
#include "stubs_syscalls.h"
#include "stubs_task_watchdog.h"
#include "stubs_vibes.h"
#include "stubs_window_manager.h"
#include "stubs_window_stack.h"

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
  return NULL;
}

void kino_reel_scale_segmented_set_deflate_effect(KinoReel *reel, int16_t expand) {}

bool kino_reel_scale_segmented_set_delay_by_distance(KinoReel *reel, GPoint target) {
  return false;
}

uint16_t time_ms(time_t *tloc, uint16_t *out_ms) { return 0; }

// Helper Functions
/////////////////////

#include "../graphics/test_graphics.h"
#include "../graphics/util.h"

// Setup and Teardown
////////////////////////////////////

static FrameBuffer *fb = NULL;

void test_expandable_dialog__initialize(void) {
  fb = malloc(sizeof(FrameBuffer));
  framebuffer_init(fb, &(GSize) {DISP_COLS, DISP_ROWS});
  // Must use System init mode to enable orphan avoidance algorithm
  const GContextInitializationMode context_init_mode = GContextInitializationMode_System;
  graphics_context_init(&s_ctx, fb, context_init_mode);
  framebuffer_clear(fb);

  // Setup resources
  fake_spi_flash_init(0, 0x1000000);
  pfs_init(false);
  pfs_format(true /* write erase headers */);
  load_resource_fixture_in_flash(RESOURCES_FIXTURE_PATH, SYSTEM_RESOURCES_FIXTURE_NAME,
                                 false /* is_next */);

  resource_init();
}

void test_expandable_dialog__cleanup(void) {
  free(fb);
}

// Helpers
//////////////////////

void prv_manual_scroll(ScrollLayer *scroll_layer, int8_t dir);

void prv_push_and_render_expandable_dialog(ExpandableDialog *expandable_dialog,
                                           uint32_t num_times_to_scroll_down) {
  Dialog *dialog = expandable_dialog_get_dialog(expandable_dialog);
  dialog_set_timeout(dialog, DIALOG_TIMEOUT_INFINITE /* no timeout */);

  window_set_on_screen(&dialog->window, true, true);

  for (uint32_t i = 0; i < num_times_to_scroll_down; i++) {
    scroll_layer_scroll(&expandable_dialog->scroll_layer, ScrollDirectionDown,
                        false /* animated */);
  }

  window_render(&dialog->window, &s_ctx);
}

// Tests
//////////////////////

void test_expandable_dialog__dismiss_tutorial_portuguese_orphan(void) {
  const char* tutorial_msg = "Remova rapidamente todas as notificações ao segurar o botão Select "
                             "durante 2 segundos a partir de qualquer notificação.";

  ExpandableDialog *expandable_dialog = expandable_dialog_create_with_params(
    "Dismiss First Use", RESOURCE_ID_QUICK_DISMISS, tutorial_msg,
    gcolor_legible_over(GColorLightGray), GColorLightGray, NULL,
    RESOURCE_ID_ACTION_BAR_ICON_CHECK, NULL);

  // Scroll down to the last page where we will observe the orphan avoidance effect
  const uint32_t num_times_to_scroll_down = 2;
  prv_push_and_render_expandable_dialog(expandable_dialog, num_times_to_scroll_down);

  cl_check(gbitmap_pbi_eq(&s_ctx.dest_bitmap, TEST_PBI_FILE));
}
