/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/util/size.h>

#include <applib/ui/window_private.h>
#include <clar.h>

// Fakes
/////////////////////

#include <fake_content_indicator.h>
#include <fake_pbl_std.h>
#include <fake_regular_timer.h>
#include <fake_rtc.h>
#include <fake_spi_flash.h>
#include <fake_workout_service.h>
#include <fixtures/load_test_resources.h>

// Stubs
/////////////////////

#include <stubs_action_menu.h>
#include <stubs_activity.h>
#include <stubs_animation_timing.h>
#include <stubs_app_window_stack.h>
#include <stubs_attribute.h>
#include <stubs_health_service.h>
#include <stubs_notifications.h>
#include <stubs_pebble_process_info.h>
#include <stubs_timeline_item.h>

typedef struct KinoReel KinoReel;

KinoReel *kino_reel_scale_segmented_create(KinoReel *from_reel, bool take_ownership,
                                           GRect screen_frame) {
  return nullptr;
}

void kino_reel_scale_segmented_set_deflate_effect(KinoReel *reel, int16_t expand) {
}

bool kino_reel_scale_segmented_set_delay_by_distance(KinoReel *reel, GPoint target) {
  return false;
}

// Helper Functions
/////////////////////

#include <fw/graphics/util.h>
