/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "popups/timeline/peek.h"
#include "pbl/services/timeline/peek.h"
#include "pbl/kernel/compiler.h"

unsigned int PBL_WEAK timeline_peek_get_concurrent_height(unsigned int num_concurrent) {
  return 0;
}

int16_t PBL_WEAK timeline_peek_get_origin_y(void) {
  return DISP_ROWS;
}

int16_t PBL_WEAK timeline_peek_get_obstruction_origin_y(void) {
  return DISP_ROWS;
}

void PBL_WEAK timeline_peek_handle_process_start(void) {
}

void PBL_WEAK timeline_peek_handle_process_kill(void) {
}

void PBL_WEAK timeline_peek_set_show_before_time(unsigned int before_time_s) {};

void PBL_WEAK timeline_peek_set_enabled(bool enabled) {
}

bool PBL_WEAK timeline_peek_prefs_get_enabled() {
  return true;
}

uint16_t PBL_WEAK timeline_peek_prefs_get_before_time() {
  return (TIMELINE_PEEK_DEFAULT_SHOW_BEFORE_TIME_S / SECONDS_PER_MINUTE);
}

void PBL_WEAK timeline_peek_prefs_set_before_time(uint16_t before_time_m) {};

void PBL_WEAK peek_animations_draw_timeline_speed_lines(GContext *ctx, GPoint offset) {
}
