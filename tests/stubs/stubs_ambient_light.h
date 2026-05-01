/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/attributes.h"

void WEAK ambient_light_init(void) {
}
uint32_t WEAK ambient_light_get_light_level(void) {
	return 0;
}
void WEAK command_als_read(void) {
}
uint32_t WEAK ambient_light_get_dark_threshold(void) {
	return 0;
}
void WEAK ambient_light_set_dark_threshold(uint32_t new_threshold) {
}
bool WEAK ambient_light_is_light(void) {
	return false;
}
