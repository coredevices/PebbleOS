/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>

#include <applib/ui/vibes.h>

void PBL_WEAK vibes_long_pulse(void) {
}

void PBL_WEAK vibes_short_pulse(void) {
}

void PBL_WEAK vibes_double_pulse(void) {
}

void PBL_WEAK vibes_cancel(void) {
}

void PBL_WEAK vibes_enqueue_custom_pattern(VibePattern pattern) {
}
