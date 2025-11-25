/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

void vibes_init();

int32_t vibes_get_vibe_strength(void);
int32_t vibes_get_default_vibe_strength(void);
void vibes_set_default_vibe_strength(int32_t vibe_strength_default);

void vibe_service_set_enabled(bool enable);
