/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/vibe.h>

void vibe_init(void) {
}

void vibe_set_strength(int8_t strength) {
}

void vibe_ctl(bool on) {
}

void vibe_force_off(void) {
}

int8_t vibe_get_braking_strength(void) {
  return VIBE_STRENGTH_OFF;
}

status_t vibe_calibrate(void) {
  return E_INVALID_OPERATION;
}

uint8_t vibe_get_calibration(void) {
  return 0xFF;
}

void vibe_apply_calibration(uint8_t cali) {
}
