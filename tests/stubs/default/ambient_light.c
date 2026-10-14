/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/ambient_light.h>
#include <pbl/kernel/compiler.h>

PBL_WEAK void ambient_light_init(void) {
}

PBL_WEAK void ambient_light_prime(void) {
}

PBL_WEAK void ambient_light_release(void) {
}

PBL_WEAK void ambient_light_suspend(void) {
}

PBL_WEAK void ambient_light_resume(void) {
}

PBL_WEAK uint32_t ambient_light_get_light_level(void) {
  return 0;
}

PBL_WEAK uint32_t ambient_light_get_dark_threshold(void) {
  return 0;
}

PBL_WEAK void ambient_light_set_dark_threshold(uint32_t new_threshold) {
}

PBL_WEAK bool ambient_light_is_light(void) {
  return false;
}

PBL_WEAK uint32_t ambient_light_level_to_lux(uint32_t light_level) {
  return light_level;
}
