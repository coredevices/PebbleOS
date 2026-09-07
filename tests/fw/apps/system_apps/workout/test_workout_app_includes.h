/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>
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

typedef struct KinoReel KinoReel;

KinoReel *PBL_WEAK kino_reel_scale_segmented_create(KinoReel *from_reel, bool take_ownership,
                                                    GRect screen_frame) {
  return nullptr;
}

void PBL_WEAK kino_reel_scale_segmented_set_deflate_effect(KinoReel *reel, int16_t expand) {
}

bool PBL_WEAK kino_reel_scale_segmented_set_delay_by_distance(KinoReel *reel, GPoint target) {
  return false;
}

// Helper Functions
/////////////////////

#include <fw/graphics/util.h>
