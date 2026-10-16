/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>

#include <pbl/kernel/compiler.h>

void PBL_WEAK light_enable_interaction(void) {
}

void PBL_WEAK light_system_color_request(void) {
}

void PBL_WEAK light_system_color_release(void) {
}

void PBL_WEAK light_set_dark_threshold(uint32_t threshold) {
}

void PBL_WEAK ambient_light_is_light(void) {
}
