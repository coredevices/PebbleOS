/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

void ambient_light_init(void) {
}
void ambient_light_prime(void) {
}
void ambient_light_release(void) {
}
void ambient_light_suspend(void) {
}
void ambient_light_resume(void) {
}
uint32_t ambient_light_get_light_level(void) {
	return 0;
}
void command_als_read(void) {
}
uint32_t ambient_light_get_dark_threshold(void) {
	return 0;
}
void ambient_light_set_dark_threshold(uint32_t new_threshold) {
}
bool ambient_light_is_light(void) {
	return false;
}
